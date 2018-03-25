
//           Copyright 2016 Juha Reunanen
//
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#include "numrabw_postoffice.h"

#include "amqpcpp/include/AMQPcpp.h"

#include <memory>
#include <sstream>

#ifdef _DEBUG
#include <thread>
#endif

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

    std::unique_ptr<AMQP> amqp;
    AMQPExchange* exchange;
    AMQPQueue* queue;

#ifdef _DEBUG
    std::thread::id workerThreadId;
#endif
};

PostOffice::PostOffice(const std::string& connectString, const char* clientIdentifier)
{
	m_connectString = connectString;

	pimpl_ = new Pimpl;

	if (clientIdentifier) {
		m_clientIdentifier = clientIdentifier;
	}
	else {
		m_clientIdentifier = "unknown";
	}
}

PostOffice::~PostOffice()
{
    CloseConnection();
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
    return pimpl_->amqp.get() != nullptr;
}

void PostOffice::RegularOperations()
{
	CheckConnection();
}

void PostOffice::Subscribe(const MessageType& type)
{
	RegularOperations();

	m_mySubscriptions.insert(type);

    pimpl_->queue->Bind(exchangeName, type);
}

void PostOffice::Unsubscribe(const MessageType& type)
{
	RegularOperations();

	m_mySubscriptions.erase(type);

    pimpl_->queue->unBind(exchangeName, type);
}

void sleep_minimal()
{
#ifdef _WIN32
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
#else // _WIN32
    usleep(10000);
#endif // _WIN32
}

bool PostOffice::Receive(Message& msg, double maxSecondsToWait)
{
	RegularOperations();

	if (!IsMailboxOk()) {
		return false;
	}

    const short receiveFlags = 0; // auto-ack

    pimpl_->queue->Get(receiveFlags);

    AMQPMessage* m = pimpl_->queue->getMessage();

    const auto count = m->getMessageCount();
    if (count == -1) {
        sleep_minimal();
        return false;
    }

    msg.m_type = m->getRoutingKey();

    uint32_t messageLength = 0;
    const char *data = m->getMessage(&messageLength);
    if (messageLength > 0) {
        msg.m_text = std::string(data, data + messageLength);
        if (messageLength != msg.m_text.length()) {
            std::ostringstream error;
            error << "Message size mismatch: " << messageLength << " != " << msg.m_text.length();
            throw std::runtime_error(error.str());
        }
    }
    return true;
}

bool PostOffice::Send(const Message& msg)
{
	RegularOperations();

	if (!IsMailboxOk()) {
		SetError("Unable to send because the connection is not ok.");
		return false;
	}

    pimpl_->exchange->Publish(msg.m_text, msg.m_type);

    return true;
}

bool PostOffice::CheckConnection()
{
    if (!pimpl_->amqp) {
        try {
            pimpl_->amqp = std::make_unique<AMQP>(m_connectString);
            pimpl_->exchange = pimpl_->amqp->createExchange();
            pimpl_->exchange->Declare(exchangeName, "topic", AMQP_DURABLE);
            pimpl_->queue = pimpl_->amqp->createQueue();
            pimpl_->queue->Declare("", AMQP_EXCLUSIVE);

            for (const auto& subscription : m_mySubscriptions) {
                pimpl_->queue->Bind(exchangeName, subscription);
            }
        }
        catch (std::exception& e) {
            SetError(e.what());
            CloseConnection();
        }
    }

	return IsMailboxOk();
}

// Returns true if there's a message to be received.
bool PostOffice::WaitForActivity(double maxSecondsToWait)
{
    return true;
}

void PostOffice::Activity()
{
}

void PostOffice::CloseConnection()
{
    if (pimpl_->amqp) {
        pimpl_->amqp->closeChannel();
        pimpl_->amqp.reset();
    }
}

#ifdef _DEBUG
void PostOffice::RegisterWorkerThread()
{
    pimpl_->workerThreadId = std::this_thread::get_id();
}
#endif // _DEBUG

}
