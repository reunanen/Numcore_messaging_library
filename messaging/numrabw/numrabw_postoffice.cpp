
//           Copyright 2018 Juha Reunanen
//
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#include "numrabw_postoffice.h"

#include "LimitedSizeBuffer.h"

#include "amqpcpp/include/AMQPcpp.h"

#include "crossguid/Guid.hpp"

#include "shared_buffer/shared_buffer.h"

#include <numcfc/IdGenerator.h>

#include <messaging/claim/ThroughputStatistics.h>
#include <messaging/claim/AttributeMessage.h>

#include <memory>
#include <sstream>
#include <thread>
#include <chrono>
#include <atomic>
#include <unordered_map>

#ifdef WIN32
//#ifdef _DEBUG
#include <windows.h> // for GetUserName and OutputDebugStringA
//#endif // _DEBUG
#endif // WIN32

namespace {
    const char* exchangeName = "Numcore_messaging_library";
}

namespace numrabw {

using namespace slaim;

class PostOffice::Pimpl {
public:
    Pimpl(const char* clientIdentifier)
        : clientIdentifier(clientIdentifier ? clientIdentifier : "unknown")
    {}

    ~Pimpl()
    {}

    void RunReceiverThread(const std::string& connectString);
    void RunSenderThread(const std::string& connectString);

    // return non-zero to stop consuming (for the time being at least)
    int HandleReceivedMessage(AMQPMessage* m);

    const char* GetVersion() const;

    // can be called from the sender thread only
    slaim::Message GetStatusMessage();

    void DeclareQueue(AMQPQueue* queue) const;

    const std::string clientIdentifier;

    std::thread receiver;
    std::thread sender;
    std::atomic<bool> receiverOk = false;
    std::atomic<bool> senderOk = false;
    std::atomic<bool> killed = false;

#ifdef _DEBUG
    std::thread::id workerThreadId;
#endif

    const std::string activityRoutingKey = "numrabw_activity_" + std::string(xg::newGuid());
    const numcfc::Time timeStarted;

    struct SubscribeAction {
        bool subscribe;
        std::string messageType;
    };

    shared_buffer<SubscribeAction> pendingSubscribeActions;

    LimitedSizeBuffer<slaim::Message> recvBuffer;
    LimitedSizeBuffer<slaim::Message> sendBuffer;

    claim::ThroughputStatistics recvThroughput;
    claim::ThroughputStatistics sendThroughput;

    std::mutex errorLogMutex;
    ErrorLog errorLog;

    std::string hostname;
    numcfc::TimeElapsed teSinceHostnameLastChecked;

    std::string username;

#if 0
    struct ActivityTrigger {
        std::unique_ptr<AMQP> amqp;
        AMQPExchange* exchange = nullptr;
    };

    std::mutex activityTriggersMutex;
    std::unordered_map<std::thread::id, ActivityTrigger> activityTriggers;

    shared_buffer<size_t> activityTriggersFromReceiverThread;
#endif
};

void DeclareExchange(AMQPExchange* exchange)
{
    exchange->Declare(exchangeName, "topic", AMQP_DURABLE);
}

void PostOffice::Pimpl::DeclareQueue(AMQPQueue* queue) const
{
    queue->Declare("", AMQP_EXCLUSIVE);
    queue->Bind(exchangeName, activityRoutingKey);
}

void PostOffice::Pimpl::RunReceiverThread(const std::string& connectString)
{
    std::set<slaim::MessageType> mySubscriptions;

    while (!killed) {
        try {
            AMQP amqp(connectString);
            AMQPExchange* exchange = amqp.createExchange();
            DeclareExchange(exchange);
            AMQPQueue* queue = amqp.createQueue();
            DeclareQueue(queue);

            for (const auto& messageType : mySubscriptions) {
                queue->Bind(exchangeName, messageType);
            }

            std::function<int(AMQPMessage*)> onMessage = [this](AMQPMessage* m) {
                return HandleReceivedMessage(m);
            };

            queue->addEvent(AMQP_MESSAGE, onMessage);

            size_t trigger = 0;

            receiverOk = true;

            while (!killed) {
                SubscribeAction subscribeAction;
                while (pendingSubscribeActions.pop_front(subscribeAction)) {
                    if (subscribeAction.subscribe) {
                        mySubscriptions.insert(subscribeAction.messageType);
                        queue->Bind(exchangeName, subscribeAction.messageType);
                    }
                    else {
                        mySubscriptions.erase(subscribeAction.messageType);
                        queue->unBind(exchangeName, subscribeAction.messageType);
                    }
                }

                queue->Consume();
#if 0
                activityTriggersFromReceiverThread.push_back(trigger++);
#endif
            }
        }
        catch (std::exception& e) {
            receiverOk = false;
            std::lock_guard<std::mutex> lock(errorLogMutex);
            errorLog.SetError(e.what());
        }
    }
}

int PostOffice::Pimpl::HandleReceivedMessage(AMQPMessage* m)
{
    slaim::Message msg;
    msg.m_type = m->getRoutingKey();

    if (msg.m_type == activityRoutingKey) {
        return 1; // triggered activity
    }

    uint32_t messageLength = 0;
    const char *data = m->getMessage(&messageLength);
    if (messageLength > 0) {
        msg.m_text = std::string(data, data + messageLength);
        if (messageLength != msg.m_text.length()) {
            std::ostringstream error;
            error << "Message size mismatch: " << messageLength << " != " << msg.m_text.length();
            std::lock_guard<std::mutex> lock(errorLogMutex);
            errorLog.SetError(error.str());
            return 2;
        }
    }

    if (!recvBuffer.push_back(msg)) {
        {
            // TODO: based on priorities, consider removing some message that is already in the buffer
            std::pair<size_t, size_t> bufferSize = recvBuffer.GetItemAndByteCount();
            std::ostringstream oss;
            oss << "Unable to push to the received messages buffer! Buffer full? (Message type = " << msg.GetType() << "; the buffer currently has " << bufferSize.first << " items totaling " << (bufferSize.second / (1024.0 * 1024.0)) << " MB.)";

            std::lock_guard<std::mutex> lock(errorLogMutex);
            errorLog.SetError(oss.str());
        }

        while (!killed && !recvBuffer.push_back(msg)) {
            std::this_thread::sleep_for(std::chrono::seconds(1)); // TODO: add a blocking push_back to the buffer itself
        }

        if (!killed) {
            recvThroughput.AddThroughput(msg.GetSize());
        }
    }
    return 0;
};


void PostOffice::Pimpl::RunSenderThread(const std::string& connectString)
{
    while (!killed) {
        try {
            AMQP amqp(connectString);
            AMQPExchange* exchange = amqp.createExchange();
            DeclareExchange(exchange);

            senderOk = true;

            auto nextStatusMessageTime = std::chrono::steady_clock::now();

            while (!killed) {
                slaim::Message msg;
                if (sendBuffer.pop_front(msg, 1.0)) {
                    exchange->Publish(msg.m_text, msg.m_type);
                    sendThroughput.AddThroughput(msg.GetSize());
                }

                const auto now = std::chrono::steady_clock::now();
                if (nextStatusMessageTime >= now) {
                    slaim::Message statusMessage = GetStatusMessage();
                    exchange->Publish(statusMessage.m_text, statusMessage.m_type);
                    nextStatusMessageTime = now;
                }
            }
        }
        catch (std::exception& e) {
            std::lock_guard<std::mutex> lock(errorLogMutex);
            errorLog.SetError(e.what());
            senderOk = false;
        }
    }
}

slaim::Message PostOffice::Pimpl::GetStatusMessage() { // can be called from the sender thread only
    claim::AttributeMessage amsg;

    if (hostname.empty() || teSinceHostnameLastChecked.GetElapsedSeconds() >= 60) {
        // avoid calling the rather expensive GetHostname() too often...
        hostname = numcfc::GetHostname();
        teSinceHostnameLastChecked.ResetToCurrent();

#ifdef WIN32
        const int maxbufsize = 32767;
        DWORD sz = maxbufsize;
        char temp[maxbufsize + 1];
        if (GetUserNameA(temp, &sz)) {
            username = temp;
        }
#else // WIN32
        char temp[L_cuserid + 1] = { 0 };
        cuserid(temp);
        m_username = temp;
#endif // WIN32
    }

    amsg.m_attributes["client_address"] = clientIdentifier;
    amsg.m_attributes["hostname"] = hostname;
    amsg.m_attributes["username"] = username;
    amsg.m_attributes["postoffice_version"] = GetVersion();

    std::pair<size_t, size_t> recvBufferSize = recvBuffer.GetItemAndByteCount();
    std::pair<size_t, size_t> sendBufferSize = sendBuffer.GetItemAndByteCount();

    assert((recvBufferSize.first > 0) == (recvBufferSize.second > 0));
    assert((sendBufferSize.first > 0) == (sendBufferSize.second > 0));

    {
        std::ostringstream oss;
        oss << recvBufferSize.first;
        amsg.m_attributes["recv_buf_item_count"] = oss.str();
    }
    {
        std::ostringstream oss;
        oss << recvBufferSize.second;
        amsg.m_attributes["recv_buf_byte_count"] = oss.str();
    }
    {
        std::ostringstream oss;
        oss << sendBufferSize.first;
        amsg.m_attributes["send_buf_item_count"] = oss.str();
    }
    {
        std::ostringstream oss;
        oss << sendBufferSize.second;
        amsg.m_attributes["send_buf_byte_count"] = oss.str();
    }

    // collect also some stats on sent/received msgs/bytes per sec (given a 10-sec window)
    std::pair<double, double> recvThroughputPerSec = recvThroughput.GetThroughputPerSec();
    std::pair<double, double> sendThroughputPerSec = sendThroughput.GetThroughputPerSec();
    {
        std::ostringstream oss;
        oss << recvThroughputPerSec.first;
        amsg.m_attributes["recv_items_per_sec"] = oss.str();
    }
    {
        std::ostringstream oss;
        oss << recvThroughputPerSec.second;
        amsg.m_attributes["recv_bytes_per_sec"] = oss.str();
    }
    {
        std::ostringstream oss;
        oss << sendThroughputPerSec.first;
        amsg.m_attributes["sent_items_per_sec"] = oss.str();
    }
    {
        std::ostringstream oss;
        oss << sendThroughputPerSec.second;
        amsg.m_attributes["sent_bytes_per_sec"] = oss.str();
    }

    numcfc::Time now;
    now.InitCurrentUniversal();
    amsg.m_attributes["time_current_utc"] = now.ToExtendedISO();
    amsg.m_attributes["time_started_utc"] = timeStarted.ToExtendedISO();
    amsg.m_attributes["working_dir"] = numcfc::GetWorkingDirectory();

    amsg.m_type = "__claim_MsgStatus";
    return amsg.GetRawMessage();
}

PostOffice::PostOffice(const std::string& connectString, const char* clientIdentifier)
    : connectString(connectString)
{
	pimpl_ = new Pimpl(clientIdentifier);
    pimpl_->receiver = std::thread(&Pimpl::RunReceiverThread, pimpl_, connectString);
    pimpl_->sender = std::thread(&Pimpl::RunSenderThread, pimpl_, connectString);
}

PostOffice::~PostOffice()
{
    pimpl_->killed = true;
    Activity();
    pimpl_->receiver.join();
    pimpl_->sender.join();
    delete pimpl_;
}

const char* PostOffice::Pimpl::GetVersion() const
{
    return "3.0 - " __TIMESTAMP__;
}

const char* PostOffice::GetVersion() const
{
    return pimpl_->GetVersion();
}

bool PostOffice::IsOk() const
{
    return pimpl_->receiverOk && pimpl_->senderOk;
}

void PostOffice::Subscribe(const MessageType& type)
{
    Pimpl::SubscribeAction subscribeAction;
    subscribeAction.messageType = type;
    subscribeAction.subscribe = true;
    pimpl_->pendingSubscribeActions.push_back(subscribeAction);
    Activity();
}

void PostOffice::Unsubscribe(const MessageType& type)
{
    Pimpl::SubscribeAction subscribeAction;
    subscribeAction.messageType = type;
    subscribeAction.subscribe = false;
    pimpl_->pendingSubscribeActions.push_back(subscribeAction);
    Activity();
}

bool PostOffice::Receive(Message& msg, double maxSecondsToWait)
{
    return pimpl_->recvBuffer.pop_front(msg, maxSecondsToWait);
}

bool PostOffice::Send(const Message& msg)
{
    bool retVal = pimpl_->sendBuffer.push_back(msg);
    if (!retVal) {
        std::pair<size_t, size_t> bufferSize = pimpl_->sendBuffer.GetItemAndByteCount();
        std::ostringstream oss;
        oss << "Unable to push to the messages being sent buffer! Buffer full? (Message type = " << msg.GetType() << "; the buffer currently has " << bufferSize.first << " items totaling " << (bufferSize.second / (1024.0 * 1024.0)) << " MB.)";

        std::lock_guard<std::mutex> lock(pimpl_->errorLogMutex);
        pimpl_->errorLog.SetError(oss.str());
    }
    return retVal;
}

#if 0
// Returns true if there's a message to be received.
bool PostOffice::WaitForActivity(double maxSecondsToWait)
{
    assert(pimpl_->workerThreadId == std::this_thread::get_id());

    bool received = false;
    size_t dummy;
    std::chrono::microseconds maxDuration(static_cast<long long>(std::round(maxSecondsToWait * 1e-6)));
    if (pimpl_->activityTriggersFromReceiverThread.pop_front(dummy, maxDuration)) {
        received = true;
    }
    if (received) {
        while (pimpl_->activityTriggersFromReceiverThread.pop_front(dummy)) {
            ;
        }
    }
    return received;
}
#endif

void PostOffice::Activity()
{
    try {
        AMQP amqp(connectString);
        AMQPExchange* exchange = amqp.createExchange();
        DeclareExchange(exchange);
        exchange->Publish("", pimpl_->activityRoutingKey);
    }
    catch (std::exception& e) {
        std::lock_guard<std::mutex> lock(pimpl_->errorLogMutex);
        pimpl_->errorLog.SetError(e.what());
    }
}

std::string PostOffice::GetError()
{
    std::lock_guard<std::mutex> lock(pimpl_->errorLogMutex);
    return pimpl_->errorLog.GetError();
}

std::string PostOffice::GetClientAddress() const
{
    return pimpl_->clientIdentifier;
}

void PostOffice::ReadSettings(claim::PostOfficeInitializer& initializer)
{
    size_t recvBufferMaxItemCount = initializer.GetReceiveBufferMaxItemCount();
    double recvBufferMaxMegabytes = initializer.GetReceiveBufferMaxMegabytes();
    size_t sendBufferMaxItemCount = initializer.GetSendBufferMaxItemCount();
    double sendBufferMaxMegabytes = initializer.GetSendBufferMaxMegabytes();

    pimpl_->recvBuffer.SetMaxItemCount(recvBufferMaxItemCount);
    pimpl_->recvBuffer.SetMaxByteCount(static_cast<size_t>(recvBufferMaxMegabytes * 1024 * 1024));
    pimpl_->sendBuffer.SetMaxItemCount(sendBufferMaxItemCount);
    pimpl_->sendBuffer.SetMaxByteCount(static_cast<size_t>(sendBufferMaxMegabytes * 1024 * 1024));
}

}
