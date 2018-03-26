
//           Copyright 2007-2008 Juha Reunanen
//                     2008-2011 Numcore Ltd
//                     2016      Juha Reunanen
//
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#ifdef WIN32
#pragma warning (disable: 4786)
#endif

#include "PostOffice.h"
#include "AttributeMessage.h"
#include "ThroughputStatistics.h"
#include <messaging/numrabw/numrabw_postoffice.h>
#include <numcfc/Time.h>
#include <numcfc/ThreadRunner.h>
#include <numcfc/IdGenerator.h>

#include <memory>
#include <condition_variable>

#include <deque>
#include <mutex>
#include <sstream>
#include <assert.h>

#ifdef WIN32
//#ifdef _DEBUG
#include <windows.h> // for GetUserName and OutputDebugStringA
//#endif // _DEBUG
#endif // WIN32

namespace claim {

//! A buffering post office implementing the standard slaim::PostOffice interface.
class BufferedPostOffice 
	: public slaim::PostOffice
	, public numcfc::ThreadRunner 
{
public:
	BufferedPostOffice(std::shared_ptr<slaim::ExtendedPostOffice> postOffice, PostOfficeInitializer& initializer);
	virtual ~BufferedPostOffice() {
		AskThreadToStop();
		m_postOffice->Activity();
		JoinThread();
	}

	virtual void Subscribe(const slaim::MessageType& t) {
		SendBufferAction action(t, true);
		bool success = m_sendBuffer.push_back(action);
		if (!success) {
			SetUnableToSendError("Subscribing for '" + t + "'");
		}
		if (success) {
			m_postOffice->Activity();
		}
	}
	virtual void Unsubscribe(const slaim::MessageType& t) { 
		SendBufferAction action(t, false);
		bool success = m_sendBuffer.push_back(action);
		if (!success) {
			SetUnableToSendError("Unsubscribing '" + t + "'");
		}
		if (success) {
			m_postOffice->Activity();
		}
	}
    
	virtual bool Send(const slaim::Message& msg) {
		bool success = m_sendBuffer.push_back(SendBufferAction(msg));
		if (!success) {
			SetUnableToSendError("Message type = " + msg.GetType());
		}
		if (success) {
			m_postOffice->Activity();
		}
		return success;
	}
	virtual bool Receive(slaim::Message& msg, double maxSecondsToWait = 0) {
		return m_recvBuffer.pop_front(msg, maxSecondsToWait);
	}
	//virtual void SetConnectInfo(const std::string& connectString) {
	//	SendBufferAction action = SendBufferAction::CreateSetConnectInfoAction(connectString);
	//	bool success = m_sendBuffer.push_back(action);
	//	if (success) {
	//		m_postOffice->Activity();
	//	}
	//	else {
	//		SetUnableToSendError("Set connect info");
	//	}
	//}
	virtual void SetClientIdentifier(const std::string& clientIdentifier) {
#ifdef WIN32
#ifdef _DEBUG
		OutputDebugStringA("\n--> NB: The method SetClientIdentifier has been deprecated!"
                           "\n--> Please supply the client identifier when creating the post office.\n\n");
#endif // _DEBUG
#endif // WIN32
		SendBufferAction action = SendBufferAction::CreateSetClientIdentifierAction(clientIdentifier);
		bool success = m_sendBuffer.push_back(action);
		if (success) {
			m_postOffice->Activity();
		}
		else {
			SetUnableToSendError("Set client identifier");
		}
	}
	virtual std::string GetClientAddress() const {
		std::unique_lock<std::mutex> lock(m_clientAddressMutex);
		return m_clientAddress;
	}

	virtual const char* GetVersion() const {
		return m_version.c_str(); // never updated in the worker thread
	}

	virtual bool SetError(const std::string& strError) {
		std::unique_lock<std::mutex> lock(m_mutexErrors);
		return ErrorLog::SetError(strError);
	}

	virtual std::string GetError() {
		std::unique_lock<std::mutex> lock(m_mutexErrors);
		return ErrorLog::GetError();
	}

private:
	BufferedPostOffice(); // no default constructor

	void UpdateVersion() {
		m_version = std::string("claimed<"); 
		m_version += m_postOffice->GetVersion(); 
		m_version += ">";
	}

	void ReadSettings(PostOfficeInitializer& initializer) {
		size_t recvBufferMaxItemCount = initializer.GetReceiveBufferMaxItemCount();
		double recvBufferMaxMegabytes = initializer.GetReceiveBufferMaxMegabytes();
		size_t sendBufferMaxItemCount = initializer.GetSendBufferMaxItemCount();
		double sendBufferMaxMegabytes = initializer.GetSendBufferMaxMegabytes();

		m_recvBuffer.SetMaxItemCount(recvBufferMaxItemCount);
		m_recvBuffer.SetMaxByteCount(static_cast<size_t>(recvBufferMaxMegabytes * 1024 * 1024));
		m_sendBuffer.SetMaxItemCount(sendBufferMaxItemCount);
		m_sendBuffer.SetMaxByteCount(static_cast<size_t>(sendBufferMaxMegabytes * 1024 * 1024));
	}

	void SetUnableToSendError(const std::string& task) {
		std::pair<size_t, size_t> bufferSize = m_sendBuffer.GetItemAndByteCount();
		std::ostringstream oss;
		oss << "Unable to push to the messages being sent buffer! Buffer full? (" << task << "; the buffer currently has " << bufferSize.first << " items totaling " << (bufferSize.second / (1024.0 * 1024.0)) << " MB.)";
		SetError(oss.str());
	}

	std::shared_ptr<slaim::ExtendedPostOffice> m_postOffice;
	std::string m_timeStarted;
	std::string m_version;

	mutable std::mutex m_clientAddressMutex;
	std::string m_clientAddress;

	numcfc::TimeElapsed m_teSinceHostnameLastChecked;
	std::string m_hostname;
	std::string m_username;

	class SendBufferAction {
	public:
		SendBufferAction() : m_actionType(UnknownAction) {}
		SendBufferAction(const slaim::Message& msg) : m_actionType(SendAction), m_msg(msg) {}
		SendBufferAction(const slaim::MessageType& type, bool subscribe) : m_actionType(SubscribeAction), m_type(type), m_subscribe(subscribe) {}

		//static SendBufferAction CreateSetConnectInfoAction(const std::string& connectInfo) { SendBufferAction action; action.m_actionType = SetConnectInfoAction; action.m_connectInfo = connectInfo; return action; }
		static SendBufferAction CreateSetClientIdentifierAction(const std::string& clientIdentifier) { SendBufferAction action; action.m_actionType = SetClientIdentifierAction; action.m_clientIdentifier = clientIdentifier; return action; }

		virtual bool Execute(slaim::PostOffice& postOffice) {
			switch (m_actionType) {
				case SendAction:
					return postOffice.Send(m_msg);
				case SubscribeAction:
					if (m_subscribe) {
						postOffice.Subscribe(m_type);
					}
					else {
						postOffice.Unsubscribe(m_type);
					}
					return true;
				case SetClientIdentifierAction:
					postOffice.SetClientIdentifier(m_clientIdentifier);
					return true;
				//case SetConnectInfoAction:
				//	postOffice.SetConnectInfo(m_connectInfo);
				//	return true;
				default:
					assert(false);
					return false;
			}
		}
		virtual size_t GetSize() const { 
			switch (m_actionType) {
				case SendAction:
					return m_msg.GetSize();
				case SubscribeAction:
					return m_type.length();
				case SetClientIdentifierAction:
					return m_clientIdentifier.length();
				//case SetConnectInfoAction:
				//	return m_connectInfo.length();
				default:
					assert(false);
					return 0;
			}
		}
	private:
		enum ActionType {
			UnknownAction,
			SendAction,
			SubscribeAction,
			SetClientIdentifierAction,
			//SetConnectInfoAction
		};

		ActionType m_actionType;

		slaim::Message m_msg;
		slaim::MessageType m_type;
		bool m_subscribe;
		std::string m_clientIdentifier;
		std::string m_connectInfo;
	};


	template <typename T>
	class LimitedSizeBuffer {
	public:
		LimitedSizeBuffer() : m_maxItemCount(1024), m_maxByteCount(1024*1024), m_currentByteCount(0) {}

		void SetMaxItemCount(size_t maxItemCount) { m_maxItemCount = maxItemCount; }
		void SetMaxByteCount(size_t maxByteCount) { m_maxByteCount = maxByteCount; }

		bool push_back(const T& item) {
			std::unique_lock<std::mutex> lock(m_mutex);
			if (m_items.size() >= m_maxItemCount) {
				return false;
			}
			else if (m_currentByteCount + item.GetSize() >= m_maxByteCount && m_items.size() > 0) { // exception: allow large messages if the buffer is otherwise empty
				return false;
			}
			m_items.push_back(item);
			m_currentByteCount += item.GetSize();

			{ // signaling
				{
					std::lock_guard<std::mutex> lock(m_mutexSignaling);
					m_notified = true;
				}
				m_condSignaling.notify_one();	
			}

			return true;
		}
		bool pop_front(T& item, double maxSecondsToWait = 0) {
			if (m_items.empty()) {
				if (maxSecondsToWait <= 0) {
					return false;
				}
				{ // signaling
					numcfc::TimeElapsed te;
					te.ResetToCurrent();
					std::unique_lock<std::mutex> lock(m_mutexSignaling);
					double secondsLeft = maxSecondsToWait;
					while (!m_notified && secondsLeft > 0) {
						m_condSignaling.wait_for(lock, std::chrono::milliseconds(static_cast<int>(secondsLeft * 1000)));
						if (!m_notified) {
							secondsLeft = maxSecondsToWait - te.GetElapsedSeconds();
							if (secondsLeft > 0.0 && secondsLeft <= 1.0) {
								numcfc::SleepMinimal(); // this is to prevent another loop in the normal case of returning false
								secondsLeft = maxSecondsToWait - te.GetElapsedSeconds();
							}
						}
					}
					if (m_notified) {
						m_notified = false;
					}
					else {
						return false;
					}
				}
			}

			std::unique_lock<std::mutex> lock(m_mutex);
			if (m_items.empty()) {
				return false; // somebody else got it
			}
			item = m_items.front();
			size_t newByteCount = m_currentByteCount - item.GetSize();
			assert(newByteCount <= m_currentByteCount);
			m_currentByteCount = newByteCount;
			m_items.pop_front();
			assert((m_currentByteCount == 0) == m_items.empty());
			return true;
		}
		std::pair<size_t, size_t> GetItemAndByteCount() const {
			std::unique_lock<std::mutex> lock(m_mutex);
			std::pair<size_t, size_t> p(std::make_pair(m_items.size(), m_currentByteCount));
			return p;
		}

	private:
		mutable std::mutex m_mutex;
		
		// for signaling
		mutable bool m_notified;
		mutable std::mutex m_mutexSignaling;
		mutable std::condition_variable m_condSignaling;
		
		std::deque<T> m_items;
		size_t m_maxItemCount;
		size_t m_maxByteCount;
		size_t m_currentByteCount;
	};

	LimitedSizeBuffer<slaim::Message> m_recvBuffer;
	LimitedSizeBuffer<SendBufferAction> m_sendBuffer;

	std::mutex m_mutexErrors;

	ThroughputStatistics m_recvThroughput;
	ThroughputStatistics m_sendThroughput;

	void operator()() {
		SendBufferAction action;
		bool messagePendingToBeSent = false;

		slaim::Message msgBeingReceived;
		bool messagePendingToBeReceived = false;

		numcfc::TimeElapsed teSinceStatusMessageSent;
		teSinceStatusMessageSent.ResetToPast();

		const int maxSendActivity = 100;
		const int maxReceiveActivity = 100;

		int sendActivity = 0, receiveActivity = 0;

#ifdef _DEBUG
		m_postOffice->RegisterWorkerThread();
#endif // _DEBUG

		while (!IsSupposedToStop()) {

			// 1. send stuff; this is on purpose done right before sending the status message

			sendActivity = 0;

			while (sendActivity < maxSendActivity) {
				bool didSomething = false;

				if (!messagePendingToBeSent) {
					messagePendingToBeSent = m_sendBuffer.pop_front(action);
					if (messagePendingToBeSent) {
						didSomething = true;
					}
				}
				if (messagePendingToBeSent) {
					if (action.Execute(*m_postOffice)) {
						m_sendThroughput.AddThroughput(action.GetSize());
						messagePendingToBeSent = false;
						didSomething = true;
					}
				}

				if (didSomething) {
					++sendActivity;
				}
				else {
					break;
				}
			}

			// 2. send status message, if appropriate

			bool sendStatusMessage = false;
			{
				bool activity = sendActivity || receiveActivity;
				double secsElapsed = teSinceStatusMessageSent.GetElapsedSeconds();
				if (!activity && secsElapsed >= 1.0) {
					sendStatusMessage = true;
				}
				else if (secsElapsed >= 5.0) {
					sendStatusMessage = true;
				}
			}

			if (sendStatusMessage) {
				teSinceStatusMessageSent.ResetToCurrent();
				slaim::Message msgStatus = GetStatusMessage();
				m_postOffice->Send(msgStatus);
			}

			// 3. receive stuff; this is on purpose done right after sending status message 

			receiveActivity = 0;

			while (receiveActivity < maxReceiveActivity) {
				bool didSomething = false;

				if (!messagePendingToBeReceived) {
					if (m_postOffice->Receive(msgBeingReceived)) {
						m_recvThroughput.AddThroughput(msgBeingReceived.GetSize());
						messagePendingToBeReceived = true;
						didSomething = true;
					}
				}

				if (messagePendingToBeReceived) {
					if (m_recvBuffer.push_back(msgBeingReceived)) {
						messagePendingToBeReceived = false;
						didSomething = true;
					}
					else {
						// TODO: based on priorities, consider removing some message that is already in the buffer
						std::pair<size_t, size_t> bufferSize = m_recvBuffer.GetItemAndByteCount();
						std::ostringstream oss;
						oss << "Unable to push to the received messages buffer! Buffer full? (Message type = " << msgBeingReceived.GetType() << "; the buffer currently has " << bufferSize.first << " items totaling " << (bufferSize.second / (1024.0 * 1024.0)) << " MB.)";
						SetError(oss.str());
					}
				}

				if (didSomething) {
					++receiveActivity;
				}
				else {
					break;
				}
			}

			// 4. update client address
			{
				std::unique_lock<std::mutex> lock(m_clientAddressMutex);
				m_clientAddress = m_postOffice->GetClientAddress();
			}

			// 5. check for errors
			for (;;) {
				std::string strError = m_postOffice->GetError();
				if (!strError.empty()) {
					SetError(strError);
				}
				else {
					break;
				}
			}

			if (sendActivity || receiveActivity) {
				;
			}
			else {
				numcfc::SleepMinimal();
			}
		}
	}

	slaim::Message GetStatusMessage() { // can be called from the post office handler thread only
		claim::AttributeMessage amsg;

		if (m_hostname.empty() || m_teSinceHostnameLastChecked.GetElapsedSeconds() >= 60) {
			// avoid calling the rather expensive GetHostname() too often...
			m_hostname = numcfc::GetHostname();
			m_teSinceHostnameLastChecked.ResetToCurrent();

#ifdef WIN32
			const int maxbufsize = 32767;
			DWORD sz = maxbufsize;
			char temp[maxbufsize + 1];
			if (GetUserNameA(temp, &sz)) {
				m_username = temp;
			}
#else // WIN32
			char temp[L_cuserid + 1] = { 0 };
			cuserid(temp);
			m_username = temp;
#endif // WIN32
		}

		amsg.m_attributes["client_address"] = m_postOffice->GetClientAddress();
		amsg.m_attributes["hostname"] = m_hostname;
		amsg.m_attributes["username"] = m_username;
		amsg.m_attributes["postoffice_version"] = GetVersion();

		std::pair<size_t, size_t> recvBufferSize = m_recvBuffer.GetItemAndByteCount();
		std::pair<size_t, size_t> sendBufferSize = m_sendBuffer.GetItemAndByteCount();

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
		std::pair<double, double> recvThroughput = m_recvThroughput.GetThroughputPerSec();
		std::pair<double, double> sendThroughput = m_sendThroughput.GetThroughputPerSec();
		{
			std::ostringstream oss;
			oss << recvThroughput.first;
			amsg.m_attributes["recv_items_per_sec"] = oss.str();
		}
		{
			std::ostringstream oss;
			oss << recvThroughput.second;
			amsg.m_attributes["recv_bytes_per_sec"] = oss.str();
		}
		{
			std::ostringstream oss;
			oss << sendThroughput.first;
			amsg.m_attributes["sent_items_per_sec"] = oss.str();
		}
		{
			std::ostringstream oss;
			oss << sendThroughput.second;
			amsg.m_attributes["sent_bytes_per_sec"] = oss.str();
		}
		
		numcfc::Time now;
		now.InitCurrentUniversal();
		amsg.m_attributes["time_current_utc"] = now.ToExtendedISO();
		amsg.m_attributes["time_started_utc"] = m_timeStarted;
		amsg.m_attributes["working_dir"] = numcfc::GetWorkingDirectory();

		amsg.m_type = "__claim_MsgStatus";
		return amsg.GetRawMessage();
	}
};

std::shared_ptr<slaim::PostOffice> CreatePostOffice(PostOfficeInitializer& initializer, const char* clientIdentifier) {
	std::string host = initializer.GetMessagingServerHost();
	int port = initializer.GetMessagingServerPort();

    // TODO: add username and password

	std::ostringstream oss;
	oss << "";
	if (host.empty()) {
		oss << "localhost";
	}
	else {
		oss << host;
	}
	oss << ":" << port;
	std::string connectInfo = oss.str();

	std::shared_ptr<slaim::ExtendedPostOffice> bpo(new numrabw::PostOffice(connectInfo, clientIdentifier));

	//bpo->SetConnectInfo(connectInfo);
	if (clientIdentifier) {
		bpo->SetClientIdentifier(clientIdentifier);
	}

	bool isBuffered = initializer.IsBuffered();

	if (isBuffered) {
		claim::BufferedPostOffice* ppo = new claim::BufferedPostOffice(bpo, initializer);
		std::shared_ptr<slaim::PostOffice> cpo(ppo);
		return cpo;
	}
	else {
		return bpo;
	}
}

BufferedPostOffice::BufferedPostOffice(std::shared_ptr<slaim::ExtendedPostOffice> postOffice, PostOfficeInitializer& initializer)
{
	m_postOffice = postOffice;

	UpdateVersion();
	ReadSettings(initializer);

	numcfc::Time now;
	now.InitCurrentUniversal();
	m_timeStarted = now.ToExtendedISO();

	StartThread();
}

class PostOffice::Impl {
public:
	std::shared_ptr<slaim::PostOffice> postOffice;
};

PostOffice::PostOffice()
{
	pimpl_ = new Impl;
}

PostOffice::~PostOffice()
{
	delete pimpl_;
}

void PostOffice::Initialize(PostOfficeInitializer& initializer, const char* clientIdentifier)
{
	if (pimpl_->postOffice.get()) {
		pimpl_->postOffice.reset(); // release the mailbox resources right here, before a new one is created... (this is RAII)
	}
	pimpl_->postOffice = CreatePostOffice(initializer, clientIdentifier);
}

void PostOffice::Initialize(numcfc::IniFile& iniFile, const char* clientIdentifier)
{
	IniFilePostOfficeInitializer initializer(iniFile);
	Initialize(initializer, clientIdentifier);
}

void PostOffice::CheckInitialized() const
{
	if (!pimpl_->postOffice.get()) {
		throw std::runtime_error("The claim::PostOffice instance has not been initialized!");
	}
}

void PostOffice::Subscribe(const slaim::MessageType& t)
{
	CheckInitialized();
	pimpl_->postOffice->Subscribe(t);
}

void PostOffice::Unsubscribe(const slaim::MessageType& t)
{
	CheckInitialized();
	pimpl_->postOffice->Unsubscribe(t);
}

bool PostOffice::Send(const slaim::Message& msg)
{
	CheckInitialized();
	return pimpl_->postOffice->Send(msg);
}

bool PostOffice::Receive(slaim::Message& msg, double maxSecondsToWait)
{
	CheckInitialized();
	CopyError();
	return pimpl_->postOffice->Receive(msg, maxSecondsToWait); 
}

void PostOffice::SetClientIdentifier(const std::string& clientIdentifier)
{
	// deprecated
	CheckInitialized();
	pimpl_->postOffice->SetClientIdentifier(clientIdentifier);
}

std::string PostOffice::GetClientAddress() const
{
	CheckInitialized();
	return pimpl_->postOffice->GetClientAddress();
}

const char* PostOffice::GetVersion() const
{
	CheckInitialized();
	return pimpl_->postOffice->GetVersion();
}

std::string PostOffice::GetError()
{
	CheckInitialized();
	CopyError();
	return slaim::PostOffice::GetError();
}

void PostOffice::CopyError()
{
	CheckInitialized();
	std::string error = pimpl_->postOffice->GetError();
	if (!error.empty()) {
		SetError(error);
	}
}

}
