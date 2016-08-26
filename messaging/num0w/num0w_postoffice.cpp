
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
#include <algorithm> // std::min
#include <chrono>

namespace num0w {

using namespace slaim;

class PostOffice::Pimpl {
public:
    Pimpl() 
        : dealer(context, ZMQ_DEALER)
    {}
    
    zmq::context_t context;
    zmq::socket_t dealer;
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

    Register();
}

PostOffice::~PostOffice()
{
    int one = 1;
    pimpl_->dealer.setsockopt(ZMQ_LINGER, &one, sizeof one);
    pimpl_->dealer.close();
	delete pimpl_;
}

const char* PostOffice::GetVersion() const
{
    return "2.0 - " __TIMESTAMP__;
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

    const auto poll = [&]() {
        zmq::pollitem_t pollItem;
        pollItem.socket = pimpl_->dealer;
        pollItem.events = ZMQ_POLLIN;

        return zmq::poll(&pollItem, 1, static_cast<long>(maxSecondsToWait * 1000)) > 0;
    };

    const auto t0 = std::chrono::high_resolution_clock::now();

    const auto waitedTooLong = [&]() {
        const auto now = std::chrono::high_resolution_clock::now();
        return std::chrono::duration_cast<std::chrono::milliseconds>(now - t0).count() >= maxSecondsToWait * 1000;
    };

    int pollResult = poll();

    if (!pollResult && !waitedTooLong()) {
        WaitForActivity(maxSecondsToWait);
        pollResult = poll();
    }

    while (pollResult || !waitedTooLong()) {
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
            SetError("No message available, even though poll() returned true");
        }

        pollResult = poll();
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

bool PostOffice::WaitForActivity(double maxSecondsToWait) const
{
    // TODO: add an in-proc socket and poll for it + the dealer socket

    sleep_minimal();
    return true;
}

void PostOffice::Activity()
{
    // TODO: send something to the abovementioned in-proc socket
}

}
