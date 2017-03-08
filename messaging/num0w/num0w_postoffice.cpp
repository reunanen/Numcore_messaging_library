
//           Copyright 2016 Juha Reunanen
//
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#include "num0w_postoffice.h"

#include "cppzmq/zmq.hpp"
#include "libzmq/src/err.hpp"

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

#define UNUSED(x) (void)(x)

#ifdef WIN32
#define USE_NONBOUND_SIGNALING_SOCKET
#else // WIN32
// TODO: implement the self-pipe trick
//       see: https://github.com/reunanen/Numcore_messaging_library/blob/c23ad1f1e7f613b29257de2a3184a7b9203c08f6/messaging/numsprew/signaling_select.cpp
#endif // WIN32

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
#ifdef USE_NONBOUND_SIGNALING_SOCKET
        , signalNonboundSocket(socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP))
#else
        , signalingListener(context, ZMQ_ROUTER)
#endif
    {}

    ~Pimpl() {
#ifdef USE_NONBOUND_SIGNALING_SOCKET
        if (signalNonboundSocket != -1) {
            closesocket(signalNonboundSocket);
            signalNonboundSocket = -1;
        }
#endif // USE_NONBOUND_SIGNALING_SOCKET
    }

#ifdef USE_NONBOUND_SIGNALING_SOCKET
    void RecreateSignalingSocket()
    {
        closesocket(signalNonboundSocket); // Probably already closed by Activity(), but this shouldn't do harm either.
        signalNonboundSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    }
#endif // USE_NONBOUND_SIGNALING_SOCKET

#ifdef _DEBUG
    std::thread::id workerThreadId;
#endif

    zmq::context_t context;
    zmq::socket_t dealer;
    std::chrono::time_point<std::chrono::high_resolution_clock> timeHeartbeatLastSent = std::chrono::high_resolution_clock::now();
    std::chrono::time_point<std::chrono::high_resolution_clock> timeLastRegistered = std::chrono::high_resolution_clock::now();

#ifdef USE_NONBOUND_SIGNALING_SOCKET
    SOCKET signalNonboundSocket;
#else
    zmq::socket_t signalingListener;
#endif
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

#ifndef USE_NONBOUND_SIGNALING_SOCKET
    pimpl_->signalingListener.bind(activitySignalingAddress);
#endif // USE_NONBOUND_SIGNALING_SOCKET

    Register();
}

PostOffice::~PostOffice()
{
    int one = 1;
    pimpl_->dealer.setsockopt(ZMQ_LINGER, &one, sizeof one);
    pimpl_->dealer.close();

#ifndef USE_NONBOUND_SIGNALING_SOCKET
    pimpl_->signalingListener.close();
#endif // USE_NONBOUND_SIGNALING_SOCKET

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

#ifdef USE_NONBOUND_SIGNALING_SOCKET
    {
        // Short-circuit based on actual dealer activity.
        zmq::pollitem_t pollItem;
        pollItem.socket = pimpl_->dealer;
        pollItem.fd = 0;
        pollItem.events = ZMQ_POLLIN;
        pollItem.revents = 0;

        if (zmq_poll(&pollItem, 1, 0) > 0) {
            return true;
        }
    }

    {
        // Check the condition of the non-bound signaling socket.
        fd_set rfds;
        FD_ZERO(&rfds);

        FD_SET(pimpl_->signalNonboundSocket, &rfds);
        size_t nfds = pimpl_->signalNonboundSocket + 1;

        struct timeval tv;
        tv.tv_sec = tv.tv_usec = 0;
        int retval = select(static_cast<int>(nfds), &rfds, NULL, &rfds, &tv);
        if (retval > 0) {
            // Shouldn't normally happen (?), because the select() timeout is zero.
            pimpl_->RecreateSignalingSocket();
            return true;
        }
        else if (retval == -1) {
            int err = GetLastError();
            {
                // The non-bound signaling socket was closed by an Activity() call in between WaitForActivity() calls.
                pimpl_->RecreateSignalingSocket();
                return true;
            }
        }
    }
#endif

    zmq::pollitem_t pollItems[2];
    pollItems[0].socket = pimpl_->dealer;
    pollItems[0].fd = 0;
    pollItems[0].events = ZMQ_POLLIN;
    pollItems[0].revents = 0;

#ifdef USE_NONBOUND_SIGNALING_SOCKET
    pollItems[1].socket = 0;
    pollItems[1].fd = pimpl_->signalNonboundSocket;
#else
    pollItems[1].socket = pimpl_->signalingListener;
    pollItems[1].fd = 0;
#endif
    pollItems[1].events = ZMQ_POLLIN;
    pollItems[1].revents = 0;

    int pollResult = 0;
    bool hasIncomingSignalingMessage = false;

    do {
        pollResult = zmq_poll(pollItems, 2, static_cast<long>(maxSecondsToWait * 1000));
        if (pollResult < 0) {
            std::ostringstream errorMessage;
            errorMessage << "Error " << zmq::errno_to_string(errno) << " from zmq_poll.";
            SetError(errorMessage.str());
        }

        hasIncomingSignalingMessage = (pollResult > 0) && (pollItems[1].revents & ZMQ_POLLIN);

        if (hasIncomingSignalingMessage) {
#ifdef USE_NONBOUND_SIGNALING_SOCKET
            // The non-bound signaling socket was closed by Activity() while we were in select().
            pimpl_->RecreateSignalingSocket();
#else // USE_NONBOUND_SIGNALING_SOCKET
            // Receive, and forget, the activity signaling message.
            zmq::message_t msgClientId, msgPayload;
            const bool receivedClientId = pimpl_->signalingListener.recv(&msgClientId, ZMQ_NOBLOCK);
            assert(receivedClientId);
            const bool receivedPayload = pimpl_->signalingListener.recv(&msgPayload, ZMQ_NOBLOCK);
            assert(receivedPayload);
            const std::string payloadContent = std::string(msgPayload.data<char>(), msgPayload.data<char>() + msgPayload.size());
            assert(activitySignalingPayload == payloadContent);

#ifndef _DEBUG
            UNUSED(receivedClientId);
            UNUSED(receivedPayload);
#endif // _DEBUG
#endif // USE_NONBOUND_SIGNALING_SOCKET

            // Reset.
            pollItems[1].revents = 0;
        }

#ifdef USE_NONBOUND_SIGNALING_SOCKET
        hasIncomingSignalingMessage = false;
#endif
    } while (hasIncomingSignalingMessage);
    
    return (pollResult > 0) && (pollItems[0].revents & ZMQ_POLLIN);
}

void PostOffice::Activity()
{
    // This looks expensive, but hopefully it is not.
    // See: http://grokbase.com/t/zeromq/zeromq-dev/13774wpwy7/how-expensive-is-an-inproc-connect

#ifdef USE_NONBOUND_SIGNALING_SOCKET
    // Simulate activity in order to make it possible for a
    // thread in WaitForActivity to immediately wake up.
    closesocket(pimpl_->signalNonboundSocket);
    pimpl_->signalNonboundSocket = -1;
#else // USE_NONBOUND_SIGNALING_SOCKET
    zmq::socket_t signalingSocket(pimpl_->context, ZMQ_DEALER);
    signalingSocket.connect(activitySignalingAddress);

    zmq::message_t request(activitySignalingPayload.data(), activitySignalingPayload.length());
    signalingSocket.send(request);

    signalingSocket.close();
#endif // USE_NONBOUND_SIGNALING_SOCKET
}

#ifdef _DEBUG
void PostOffice::RegisterWorkerThread()
{
    pimpl_->workerThreadId = std::this_thread::get_id();
}
#endif // _DEBUG

}
