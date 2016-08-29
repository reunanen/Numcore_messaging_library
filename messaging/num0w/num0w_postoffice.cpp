
//           Copyright 2016 Juha Reunanen
//
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#include "num0w_postoffice.h"

#include "cppzmq/zmq.hpp"

#include <assert.h>
#include <time.h>

#include <cstdlib>
#include <cstring>
#include <sstream>
#include <fstream>
#include <chrono>

#ifdef _DEBUG
#include <thread>
#endif // _DEBUG

namespace num0w {

using namespace slaim;

namespace {
    const char* activitySignalingAddress = "inproc://activity-signaling";
    const std::string activitySignalingPayload = "activity";
}

class PostOffice::Pimpl {
public:
    Pimpl() 
        : dealer(context, ZMQ_DEALER)
        , signalingListener(context, ZMQ_ROUTER)
    {}

#ifdef _DEBUG
    std::thread::id workerThreadId;
#endif

    zmq::context_t context;
    zmq::socket_t dealer, signalingListener;
    std::chrono::time_point<std::chrono::high_resolution_clock> timeHeartbeatLastSent = std::chrono::high_resolution_clock::now();
    std::chrono::time_point<std::chrono::high_resolution_clock> timeLastRegistered = std::chrono::high_resolution_clock::now();
};

PostOffice::PostOffice(const std::string& connectString, const char* clientIdentifier)
{
	m_connectString = connectString;

	pimpl_ = new Pimpl;

	m_tDummyMessageLastSent = time(NULL);
	if (clientIdentifier) {
		m_clientIdentifier = clientIdentifier;
	}
	else {
		m_clientIdentifier = "unknown";
	}

    const int one = 1;
    pimpl_->dealer.setsockopt(ZMQ_TCP_KEEPALIVE, &one, sizeof one);
    pimpl_->dealer.setsockopt(ZMQ_TCP_KEEPALIVE_IDLE, &one, sizeof one);
    pimpl_->dealer.setsockopt(ZMQ_TCP_KEEPALIVE_INTVL, &one, sizeof one);

    pimpl_->dealer.connect(m_connectString);

    pimpl_->signalingListener.bind(activitySignalingAddress);

    Register();
}

PostOffice::~PostOffice()
{
    int one = 1;
    pimpl_->dealer.setsockopt(ZMQ_LINGER, &one, sizeof one);
    pimpl_->dealer.close();

    pimpl_->signalingListener.close();

    delete pimpl_;
}

const char* PostOffice::GetVersion() const
{
    return "2.1 - " __TIMESTAMP__;
}

std::string ToString(const zmq::message_t& message)
{
    return std::string(static_cast<const char*>(message.data()), static_cast<const char*>(message.data()) + message.size());
}

zmq::message_t ToMessage(const std::string& string)
{
    return zmq::message_t(string.data(), string.size());
}

void PostOffice::Register()
{
    const std::string header = "Register";
    pimpl_->dealer.send(ToMessage(header), ZMQ_SNDMORE);
    pimpl_->dealer.send(ToMessage(m_clientIdentifier), 0);

    pimpl_->timeLastRegistered = std::chrono::high_resolution_clock::now();
}

void PostOffice::SetClientIdentifier(const std::string& clientIdentifier)
{
	if (clientIdentifier != m_clientIdentifier) {
		m_clientIdentifier = clientIdentifier;

        Register();
	}
}

std::string PostOffice::GetClientAddress() const
{	
    return m_clientIdentifier;
}

bool PostOffice::IsMailboxOk() const
{
    return true;
}

void PostOffice::RegularOperations()
{
	CheckConnection();

    // send keep-alive

    const auto now = std::chrono::high_resolution_clock::now();
    if (std::chrono::duration_cast<std::chrono::milliseconds>(now - pimpl_->timeHeartbeatLastSent).count() >= 1000) {
        pimpl_->dealer.send(ToMessage("Heartbeat"));
        pimpl_->timeHeartbeatLastSent = now;
    }
}

void PostOffice::Subscribe(const MessageType& type)
{
	RegularOperations();

	m_mySubscriptions.insert(type);

    const std::string header = "Subscribe";
    pimpl_->dealer.send(ToMessage(header), ZMQ_SNDMORE);
    pimpl_->dealer.send(ToMessage(type), 0);
}

void PostOffice::Unsubscribe(const MessageType& type)
{
	RegularOperations();

	m_mySubscriptions.erase(type);

    const std::string header = "Unsubscribe";
    pimpl_->dealer.send(ToMessage(header), ZMQ_SNDMORE);
    pimpl_->dealer.send(ToMessage(type), 0);
}

bool PostOffice::Receive(Message& msg, double maxSecondsToWait)
{
	RegularOperations();

	if (!IsMailboxOk()) {
		return false;
	}

    const auto t0 = std::chrono::high_resolution_clock::now();

    const auto secondsRemaining = [&t0, maxSecondsToWait]() {
        const auto now = std::chrono::high_resolution_clock::now();
        const double secondsElapsed = std::chrono::duration_cast<std::chrono::microseconds>(now - t0).count() * 1e-6;
        return (std::max)(0.0, maxSecondsToWait - secondsElapsed);
    };

    while (WaitForActivity(secondsRemaining())) {
        zmq::message_t headerMessage;
        if (pimpl_->dealer.recv(&headerMessage, ZMQ_DONTWAIT)) {
            std::string header = ToString(headerMessage);
            if (header == "Publish") {
                zmq::message_t messageTypeMessage;
                if (pimpl_->dealer.recv(&messageTypeMessage, ZMQ_DONTWAIT)) {
                    std::string messageType = ToString(messageTypeMessage);
                    zmq::message_t payloadMessage;
                    if (pimpl_->dealer.recv(&payloadMessage, ZMQ_DONTWAIT)) {
                        msg.m_type = messageType;
                        msg.m_text = ToString(payloadMessage);
                        return true;
                    }
                    else {
                        SetError("Sequence error: no payload");
                    }
                }
                else {
                    SetError("Sequence error: no message type");
                }
            }
            else if (header == "Register") {
                zmq::message_t clientIdentifierMessage;
                if (pimpl_->dealer.recv(&clientIdentifierMessage, ZMQ_DONTWAIT)) {
                    m_clientIdentifier = ToString(clientIdentifierMessage);
                }
                else {
                    // error
                }
            }
            else if (header == "UnregisteredError") {
                const auto now = std::chrono::high_resolution_clock::now();
                if (std::chrono::duration_cast<std::chrono::milliseconds>(now - pimpl_->timeLastRegistered).count() >= 2000) {
                    Register();

                    for (const MessageType& type : m_mySubscriptions) {
                        Subscribe(type);
                    }
                }
            }
            else {
                SetError("Sequence error: unexpected header " + header);
            }
        }
        else {
            SetError("No message available, even though WaitForActivity() returned true");
        }
    }

	return false;
}

void sleep_minimal()
{
#ifdef _WIN32
	::Sleep(10);
#else // _WIN32
	usleep(10000);
#endif // _WIN32
}

bool PostOffice::Send(const Message& msg)
{
	RegularOperations();

	if (!IsMailboxOk()) {
		SetError("Unable to send because the connection is not ok.");
		return false;
	}

	if (msg.m_type.find(" ") != std::string::npos) {
		SetError("Message type '" + msg.m_type + "' contains a tabulator!");
		return false;
	}

    pimpl_->dealer.send(ToMessage("Publish"), ZMQ_SNDMORE);
    pimpl_->dealer.send(ToMessage(msg.m_type), ZMQ_SNDMORE);
    pimpl_->dealer.send(ToMessage(msg.m_text));
    return true;
}

bool PostOffice::CheckConnection()
{
	return IsMailboxOk();
}

// Returns true if there's a message to be received.
bool PostOffice::WaitForActivity(double maxSecondsToWait)
{
#ifdef _DEBUG
    // NOTE: In debug mode, the worker thread is supposed to register by calling RegisterWorkerThread().
    assert(std::this_thread::get_id() == pimpl_->workerThreadId);
#endif

    zmq::pollitem_t pollItems[2];
    pollItems[0].socket = pimpl_->dealer;
    pollItems[0].fd = 0;
    pollItems[0].events = ZMQ_POLLIN;
    pollItems[0].revents = 0;
    pollItems[1].socket = pimpl_->signalingListener;
    pollItems[1].fd = 0;
    pollItems[1].events = ZMQ_POLLIN;
    pollItems[1].revents = 0;

    int pollResult = 0;
    bool hasIncomingSignalingMessage = false;

    do {
        pollResult = zmq::poll(pollItems, 2, static_cast<long>(maxSecondsToWait * 1000));
        hasIncomingSignalingMessage = pollResult && (pollItems[1].revents & ZMQ_POLLIN);

        if (hasIncomingSignalingMessage) {
            // Receive the activity signaling message.
            zmq::message_t msgClientId, msgPayload;
            const bool receivedClientId = pimpl_->signalingListener.recv(&msgClientId, ZMQ_NOBLOCK);
            assert(receivedClientId);
            const bool receivedPayload = pimpl_->signalingListener.recv(&msgPayload, ZMQ_NOBLOCK);
            assert(receivedPayload);
            const std::string payloadContent = std::string(msgPayload.data<char>(), msgPayload.data<char>() + msgPayload.size());
            assert(activitySignalingPayload == payloadContent);

            // Send a reply to the signaler.
            pimpl_->signalingListener.send(msgClientId, ZMQ_SNDMORE);
            pimpl_->signalingListener.send(msgPayload, 0);

            // Reset.
            pollItems[1].revents = 0;
        }
    } while (hasIncomingSignalingMessage);
    
    return pollResult && pollItems[0].revents;
}

void PostOffice::Activity()
{
    // This looks expensive, but hopefully it is not.
    // See: http://grokbase.com/t/zeromq/zeromq-dev/13774wpwy7/how-expensive-is-an-inproc-connect

    zmq::socket_t signalingSocket(pimpl_->context, ZMQ_DEALER);
    signalingSocket.connect(activitySignalingAddress);

    zmq::message_t request(activitySignalingPayload.data(), activitySignalingPayload.length());
    signalingSocket.send(request);

    // Make sure there's a reply before the socket is closed in the end (note: is this really necessary? or useful?)
    zmq::message_t reply;
    const bool receivedReply = signalingSocket.recv(&reply);
    assert(receivedReply);
    const std::string payloadContent = std::string(reply.data<char>(), reply.data<char>() + reply.size());
    assert(activitySignalingPayload == payloadContent);

    signalingSocket.close();
}

#ifdef _DEBUG
void PostOffice::RegisterWorkerThread()
{
    pimpl_->workerThreadId = std::this_thread::get_id();
}
#endif // _DEBUG

}
