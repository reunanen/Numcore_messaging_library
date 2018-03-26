
//           Copyright 2018 Juha Reunanen
//
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#include "numrabw_postoffice.h"

#include "amqpcpp/include/AMQPcpp.h"

#include "crossguid/Guid.hpp"

#include "shared_buffer/shared_buffer.h"

#include <memory>
#include <sstream>
#include <thread>
#include <chrono>
#include <atomic>
#include <unordered_map>

namespace {
    const char* exchangeName = "Numcore_messaging_library";
}

namespace numrabw {

using namespace slaim;

class PostOffice::Pimpl {
public:
    Pimpl() 
    {}

    ~Pimpl()
    {}

    void RunReceiverThread(const std::string& connectString);
    void RunSenderThread(const std::string& connectString);

    void DeclareQueue(AMQPQueue* queue) const;

    std::thread receiver;
    std::thread sender;
    std::atomic<bool> receiverOk = false;
    std::atomic<bool> senderOk = false;
    std::atomic<bool> killed = false;

#ifdef _DEBUG
    std::thread::id workerThreadId;
#endif

    const std::string activityRoutingKey = "numrabw_activity_" + std::string(xg::newGuid());

    struct SubscribeAction {
        bool subscribe;
        std::string messageType;
    };

    shared_buffer<SubscribeAction> pendingSubscribeActions;

    shared_buffer<slaim::Message> receivedMessages;
    shared_buffer<slaim::Message> messagesToBeSent;

    shared_buffer<std::string> errors;

    struct ActivityTrigger {
        std::unique_ptr<AMQP> amqp;
        AMQPExchange* exchange = nullptr;
    };

    std::mutex activityTriggersMutex;
    std::unordered_map<std::thread::id, ActivityTrigger> activityTriggers;

    shared_buffer<size_t> activityTriggersFromReceiverThread;
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

            std::function<int(AMQPMessage*)> onMessage = [this, &queue](AMQPMessage* m) {
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
                        errors.push_back(error.str());
                        return 2;
                    }
                }

                receivedMessages.push_back(msg);
                return 0;
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

                queue->Consume(AMQP_NOACK);
                activityTriggersFromReceiverThread.push_back(trigger++);
            }
        }
        catch (std::exception& e) {
            receiverOk = false;
            errors.push_back(e.what());
        }
    }
}

void PostOffice::Pimpl::RunSenderThread(const std::string& connectString)
{
    while (!killed) {
        try {
            AMQP amqp(connectString);
            AMQPExchange* exchange = amqp.createExchange();
            DeclareExchange(exchange);

            senderOk = true;

            while (!killed) {
                slaim::Message msg;
                if (messagesToBeSent.pop_front(msg)) {
                    exchange->Publish(msg.m_text, msg.m_type);
                }
            }
        }
        catch (std::exception& e) {
            errors.push_back(e.what());
            senderOk = false;
        }
    }
}


PostOffice::PostOffice(const std::string& connectString, const char* clientIdentifier)
{
	m_connectString = connectString;

	pimpl_ = new Pimpl;
    pimpl_->receiver = std::thread(&Pimpl::RunReceiverThread, pimpl_, connectString);
    pimpl_->sender = std::thread(&Pimpl::RunSenderThread, pimpl_, connectString);

	if (clientIdentifier) {
		m_clientIdentifier = clientIdentifier;
	}
	else {
		m_clientIdentifier = "unknown";
	}
}

PostOffice::~PostOffice()
{
    pimpl_->killed = true;
    Activity();
    pimpl_->messagesToBeSent.halt();
    pimpl_->receiver.join();
    pimpl_->sender.join();
    delete pimpl_;
}

const char* PostOffice::GetVersion() const
{
    return "3.0 - " __TIMESTAMP__;
}

void PostOffice::SetClientIdentifier(const std::string& clientIdentifier)
{
	if (clientIdentifier != m_clientIdentifier) {
		m_clientIdentifier = clientIdentifier;
	}
}

std::string PostOffice::GetClientAddress() const
{	
    return m_clientIdentifier;
}

bool PostOffice::IsMailboxOk() const
{
    return pimpl_->receiverOk && pimpl_->senderOk;
}

void PostOffice::RegularOperations()
{
    if (!HasError()) {
        std::string error;
        if (pimpl_->errors.pop_front(error)) {
            SetError(error);
        }
    }
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
    std::chrono::microseconds maxDuration(static_cast<long long>(std::round(maxSecondsToWait * 1e-6)));
    return pimpl_->receivedMessages.pop_front(msg, maxDuration);
}

bool PostOffice::Send(const Message& msg)
{
    assert(pimpl_->workerThreadId == std::this_thread::get_id());

	RegularOperations();

    if (pimpl_->messagesToBeSent.size() < 100000) {
        pimpl_->messagesToBeSent.push_back(msg);
        return true;
    }
    else {
        SetError("Unable to send, because the buffer is full");
        return false;
    }
}

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

void PostOffice::Activity()
{
    const auto threadId = std::this_thread::get_id();
    std::lock_guard<std::mutex> lock(pimpl_->activityTriggersMutex);
    auto i = pimpl_->activityTriggers.find(threadId);
    try {
        if (i == pimpl_->activityTriggers.end()) {
            Pimpl::ActivityTrigger& activityTrigger = pimpl_->activityTriggers[threadId];
            activityTrigger.amqp = std::make_unique<AMQP>(m_connectString);
            activityTrigger.exchange = activityTrigger.amqp->createExchange();
            DeclareExchange(activityTrigger.exchange);
            i = pimpl_->activityTriggers.find(threadId);
        }
        i->second.exchange->Publish("", pimpl_->activityRoutingKey);
    }
    catch (std::exception& e) {
        pimpl_->activityTriggers.erase(threadId);
        pimpl_->errors.push_back(e.what());
    }
}

#ifdef _DEBUG
void PostOffice::RegisterWorkerThread()
{
    pimpl_->workerThreadId = std::this_thread::get_id();
}
#endif // _DEBUG

}
