
//           Copyright 2007-2008 Juha Reunanen
//                     2008-2011 Numcore Ltd
//
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#include "numsprew_postoffice.h"

#include <sp.h>

//#include <winsock.h>
#ifdef WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <winbase.h>
#else // WIN32
#endif // WIN32

#include <assert.h>
#include <time.h>

#include <cstdlib>
#include <cstring>
#include <sstream>
#include <fstream>
#include <algorithm> // std::min

#include "signaling_select.h"

#define NO_MAILBOX -1
#define MAX_MESSLEN 102400

#define MSG_TYPE_NORMAL 1
#define MSG_TYPE_MULTI 2
#define MSG_TYPE_MULTI_START 4
#define MSG_TYPE_MULTI_END 8

namespace numsprew {

using namespace slaim;

class PostOffice::Pimpl {
public:
	mailbox m_mailbox;
	char m_privateGroup[MAX_GROUP_NAME + 1];

	SignalingSelect m_signalingSelect;

	bool m_connectionFailure;
};

PostOffice::PostOffice(const std::string& connectString, const char* clientIdentifier)
{
#ifdef WIN32
	static bool initOk = false;
	// not really thread safe!
	if (!initOk) {
		// Static WSA init
	    WSADATA ws; 
		if( WSAStartup( MAKEWORD(2,2), &ws ) != 0 ) { 			
			WSACleanup(); 
			SetError("No WinSock 2.2 support!");
		}
		else {
			initOk = true;
		}
	}
#endif // WIN32

	//m_nReceiveBufferSize = 2*1024*1024; // 2 MB
	//m_pReceiveBuffer = new char[m_nReceiveBufferSize];

	m_connectString = connectString;
        //assert(m_connectString.find("@") != std::string::npos); // no more true in Linux with IPC communication

	pimpl_ = new Pimpl;
	pimpl_->m_mailbox = NO_MAILBOX;
	pimpl_->m_connectionFailure = false;
	pimpl_->m_privateGroup[0] = '\0';

	m_tDummyMessageLastSent = time(NULL);
	if (clientIdentifier) {
		m_clientIdentifier = clientIdentifier;
	}
	else {
		m_clientIdentifier = "unknown";
	}
}

PostOffice::~PostOffice()
{
	if (pimpl_->m_mailbox != NO_MAILBOX) {
		SP_disconnect(pimpl_->m_mailbox);
	}
	delete pimpl_;
}

void strip_CVS_keyword(std::string& str)
{
	if (str.length() < 7) {
		return; // not a CVS keyword
	}
	if (str[0] != '$') {
		return; // not a CVS keyword
	}
	if (str[str.length() - 1] != '$') {
		return; // not a CVS keyword
	}
	if (str[str.length() - 2] != ' ') {
		return; // not a CVS keyword
	}

	size_t start = str.find(' ') + 1;
	if (start < 4) {
		return; // not a CVS keyword
	}
	if (start >= str.length() - 3) {
		return; // not a CVS keyword
	}

	if (str[start - 2] != ':') {
		return; // not a CVS keyword
	}
	if (str[start - 1] != ' ') {
		return; // not a CVS keyword
	}

	size_t stop = str.length() - 2;

	str = str.substr(start, stop - start);
}

const char* PostOffice::GetVersion() const
{
	//return "$Id: numsprew_postoffice.cpp,v 1.37 2011-08-30 13:24:44 juha Exp $";
	
	static std::string version;
	if (version.empty()) {
		std::string revision = "$Revision: 1.37 $";
		std::string date = "$Date: 2011-08-30 13:24:44 $";
		std::string filename = "$RCSfile: numsprew_postoffice.cpp,v $";

		strip_CVS_keyword(revision);
		strip_CVS_keyword(date);
		strip_CVS_keyword(filename);

		version = filename + " " + revision + ": " + date + ", " + __TIMESTAMP__;
	}
	return version.c_str();
}

//void PostOffice::SetConnectInfo(const std::string& connectString)
//{
//	if (connectString != m_connectString) {
//		m_connectString = connectString;
//		
//		// need to re-establish the connection in order to update the private group
//		if (pimpl_->m_mailbox != NO_MAILBOX) {
//			SP_disconnect(pimpl_->m_mailbox);
//			pimpl_->m_mailbox = NO_MAILBOX;
//		}
//	}
//}

void PostOffice::SetClientIdentifier(const std::string& clientIdentifier)
{
	if (clientIdentifier != m_clientIdentifier) {
		m_clientIdentifier = clientIdentifier;

		// need to re-establish the connection in order to update the private group
		if (pimpl_->m_mailbox != NO_MAILBOX) {
			SP_disconnect(pimpl_->m_mailbox);
			pimpl_->m_mailbox = NO_MAILBOX;
		}
	}
}

std::string PostOffice::GetClientAddress() const
{	
	return pimpl_->m_privateGroup;
}

bool PostOffice::IsMailboxOk() const
{
	return pimpl_->m_mailbox != NO_MAILBOX;
}

void PostOffice::RegularOperations()
{
	CheckConnection();

	if (IsMailboxOk()) {

	}
}

void PostOffice::Subscribe(const MessageType& type)
{
	RegularOperations();

	m_mySubscriptions.insert(type);

	int ret = SP_join(pimpl_->m_mailbox, type.c_str());

	if (ret < 0) {
		OnSpreadError(ret, "Subscribing");
	}
}

void PostOffice::Unsubscribe(const MessageType& type)
{
	RegularOperations();

	m_mySubscriptions.erase(type);

	int ret = SP_leave(pimpl_->m_mailbox, type.c_str());

	if (ret < 0) {
		OnSpreadError(ret, "Unsubscribing");
	}
}

bool PostOffice::Receive(Message& msg, double maxSecondsToWait)
{
	RegularOperations();

	if (!IsMailboxOk()) {
		return false;
	}

	int pollResult = SP_poll(pimpl_->m_mailbox);

	if (!pollResult && maxSecondsToWait > 0.0) {
		WaitForActivity(maxSecondsToWait);
		pollResult = SP_poll(pimpl_->m_mailbox);
	}

	while (IsMailboxOk() && pollResult) {
		pollResult = 0; // will be re-checked in the end of the while loop

		char sender[MAX_GROUP_NAME];
		const char MAX_GROUPS = 16;
		char groups[MAX_GROUPS][MAX_GROUP_NAME];
		int groupCount;
		int16 messageType;
		int endianMismatch;
		service serviceType = DROP_RECV;
		char buf[MAX_MESSLEN];

		int ret = SP_receive(pimpl_->m_mailbox, &serviceType, sender, MAX_GROUPS, &groupCount, groups, &messageType, &endianMismatch, MAX_MESSLEN, buf);

		if (ret > 0) {
			if (!Is_membership_mess(serviceType)) {

				if (messageType & MSG_TYPE_NORMAL || messageType & MSG_TYPE_MULTI_START) {
					void* tabPos = memchr(buf, '\t', MAX_GROUP_NAME + 1);

					if (tabPos != NULL) {
						msg.m_type = std::string((const char*) buf, (const char*) tabPos);
						msg.m_text = std::string(1 + (const char*) tabPos, ret + (const char*) buf);

						if (messageType & MSG_TYPE_NORMAL) {
							return true;
						}
						else {
							m_multiMessagesBeingReceived[sender] = msg;
						}
					}
					else {
						SetError("No tab found in the message received!?");
					}
				}

				else if (messageType & MSG_TYPE_MULTI) { // intermediate or end
					std::map<std::string, slaim::Message>::iterator i = m_multiMessagesBeingReceived.find(sender);

					if (i != m_multiMessagesBeingReceived.end()) {
						Message& msgMulti = i->second;
						msgMulti.m_text.append(buf, buf + ret);

						if (messageType & MSG_TYPE_MULTI_END) {
							msg = msgMulti;
							m_multiMessagesBeingReceived.erase(i);
							return true;
						}
					}
					else {
						SetError("Start of multi-message lost! (type = " + std::string(groups[0]) + ", sender = " + std::string(sender) + ")");
					}
				}

				else if (messageType == 0) {
					// keep-alive messages
					if (strncmp(buf, "keep_alive", 10) == 0) {
						msg.m_type = "keep_alive";
						msg.m_text = sender;
						return true;
					}
				}

				else {
                                        ; // shouldn't happen??
				}
			}
			else {
				; // shouldn't happen??
			}
		}
		else {
			break;
		}

		pollResult = SP_poll(pimpl_->m_mailbox);
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

	int errorCounter = 0;

	size_t msgLength = msg.m_type.length() + 1 + msg.m_text.length();

	if (msgLength <= MAX_MESSLEN) {
		//std::ostringstream oss;	
		//oss << msg.m_type << "\t" << msg.m_text;

		std::string str;
		str.resize(msgLength);
		memcpy(&str[0], &msg.m_type[0], msg.m_type.length());
		str[msg.m_type.length()] = '\t';
		memcpy(&str[msg.m_type.length() + 1], &msg.m_text[0], msg.m_text.length());

		while (true) { // loop until success or errorCounter >= 10
			int ret = SP_multicast(pimpl_->m_mailbox, FIFO_MESS, msg.m_type.c_str(), MSG_TYPE_NORMAL, (int) str.length(), str.data());

			if (ret == static_cast<int>(str.length())) {
				return true;
			}
			else if (ret < 0) {
				if (++errorCounter >= 10) {
					OnSpreadError(ret, "Sending");
					return false;
				}
				else {
					// wait...
					sleep_minimal();
					CheckConnection();
				}
			}
			else {
				std::ostringstream ossError;
				ossError << "Only " << ret << " out of " << str.length() << " bytes sent?!";
				SetError(ossError.str());
				return false;
			}
		}
	}
	else {

		// 1. send start-message which consists of the type only
		std::string typePlusTab = msg.m_type + "\t";

		while (true) {
			int ret = SP_multicast(pimpl_->m_mailbox, FIFO_MESS, msg.m_type.c_str(), MSG_TYPE_MULTI_START, static_cast<int>(typePlusTab.length()), typePlusTab.data());

			if (ret == static_cast<int>(typePlusTab.length())) {
				errorCounter = 0;
				break;
			}
			else if (ret < 0) {
				if (++errorCounter >= 50) {
					OnSpreadError(ret, "Send-multi");
					return false;
				}
				// wait...
				sleep_minimal();
				CheckConnection();
			}
			else {
				std::ostringstream ossError;
				ossError << "Only " << ret << " out of " << typePlusTab.length() << " bytes sent?!";
				SetError(ossError.str());
				return false;
			}
		}

		size_t bytesLeft = msg.m_text.length();
		unsigned int bytesSent = 0;

		while (bytesLeft > 0) {
			size_t msgSize = std::min<size_t>(bytesLeft, MAX_MESSLEN);

			int messageType = MSG_TYPE_MULTI;
			if (msgSize == bytesLeft) {
				messageType |= MSG_TYPE_MULTI_END;
			}

			int ret = SP_multicast(pimpl_->m_mailbox, FIFO_MESS, msg.m_type.c_str(), messageType, (int) msgSize, msg.m_text.data() + bytesSent);

			if (ret == static_cast<int>(msgSize)) {
				bytesSent += ret;
				bytesLeft -= ret;
				errorCounter = 0;
			}
			else if (ret < 0) {
				if (++errorCounter >= 50) {
					OnSpreadError(ret, "Send-multi");
					return false;
				}
				// wait...
				sleep_minimal();
				CheckConnection();
			}
			else {
				std::ostringstream ossError;
				ossError << "Only " << ret << " out of " << msgSize << " bytes sent?!";
				SetError(ossError.str());
				return false;
			}
		}

		if (bytesLeft == 0 && bytesSent == msg.m_text.length()) {
			return true;
		}
	}

	return false;
}

void PostOffice::OnSpreadError(int nSpreadError, const char* szComment /* = NULL */)
{
	std::ostringstream oss;

	bool closeMailbox = false;

	switch (nSpreadError) {
	case ILLEGAL_SPREAD: oss << "ILLEGAL_SPREAD"; closeMailbox = true; break;
	case COULD_NOT_CONNECT: oss << "COULD_NOT_CONNECT"; break;
	case REJECT_QUOTA: oss << "REJECT_QUOTA"; break;
	case REJECT_NO_NAME: oss << "REJECT_NO_NAME"; break;
	case REJECT_ILLEGAL_NAME: oss << "REJECT_ILLEGAL_NAME"; break;
	case REJECT_NOT_UNIQUE: oss << "REJECT_NOT_UNIQUE"; break;
	case REJECT_VERSION: oss << "REJECT_VERSION"; break;
	case CONNECTION_CLOSED: oss << "CONNECTION_CLOSED"; closeMailbox = true; break;

	case ILLEGAL_SESSION: oss << "ILLEGAL_SESSION"; closeMailbox = true; break;
	case ILLEGAL_SERVICE: oss << "ILLEGAL_SERVICE"; closeMailbox = true; break;
	case ILLEGAL_MESSAGE: oss << "ILLEGAL_MESSAGE"; break;
	case ILLEGAL_GROUP: oss << "ILLEGAL_GROUP"; break;
	case BUFFER_TOO_SHORT: oss << "BUFFER_TOO_SHORT"; break;

	case GROUPS_TOO_SHORT: oss << "GROUPS_TOO_SHORT"; break;
	case MESSAGE_TOO_LONG: oss << "MESSAGE_TOO_LONG"; break;
#ifdef NET_ERROR_ON_SESSION
	case NET_ERROR_ON_SESSION: oss << "NET_ERROR_ON_SESSION"; closeMailbox = true; break;
#endif // NET_ERROR_ON_SESSION

	default: oss << "UNKNOWN_ERROR(" << nSpreadError << ")"; closeMailbox = true; break;
	}

	oss << " (" << m_connectString << ")";

	if (szComment) {
		oss << " - " << szComment;
	}
	oss << ".";

	SetError(oss.str());

	if (closeMailbox && pimpl_->m_mailbox != NO_MAILBOX) {
		SP_disconnect(pimpl_->m_mailbox);
		pimpl_->m_mailbox = NO_MAILBOX;
		pimpl_->m_connectionFailure = true;
	}
}

bool PostOffice::CheckConnection()
{
	if (!IsMailboxOk()) {
		sp_time timeout;
		timeout.sec = 10;
		timeout.usec = 0;

		int priority = 0;
		int groupMembershipMessages = 0;

		unsigned int clientNumber = 0;
			
		while (!IsMailboxOk() && clientNumber < 1000) {
			std::string clientIdentifier = m_clientIdentifier;
			std::ostringstream ossSuffix;

			if (clientNumber > 0) {
				ossSuffix << "-" << clientNumber;	
			}

			if ((clientIdentifier + ossSuffix.str()).length() > MAX_PRIVATE_NAME) {
				clientIdentifier = clientIdentifier.substr(0, MAX_PRIVATE_NAME - ossSuffix.str().length());
			}
			clientIdentifier += ossSuffix.str();
			assert(clientIdentifier.length() <= MAX_PRIVATE_NAME);

			clientNumber++;

			pimpl_->m_privateGroup[0] = '\0';

			int ret = SP_connect_timeout(m_connectString.c_str(), clientIdentifier.c_str(), priority, groupMembershipMessages, &pimpl_->m_mailbox, pimpl_->m_privateGroup, timeout);

			if (ret == ACCEPT_SESSION) {
				pimpl_->m_privateGroup[MAX_GROUP_NAME] = '\0';
				break;
			}
			else if (ret != REJECT_NOT_UNIQUE) {
				OnSpreadError(ret);
				break;
			}
		}

		if (IsMailboxOk()) {
			if (pimpl_->m_connectionFailure) {
				SetError("Mailbox now ok!");
				pimpl_->m_connectionFailure = false;
			}
			for (std::set<MessageType>::const_iterator i = m_mySubscriptions.begin(); i != m_mySubscriptions.end(); i++) {
				const MessageType& type = *i;
				SP_join(pimpl_->m_mailbox, type.c_str());
			}
		}
	}
	else {
		time_t tNow = time(NULL);
		//if (difftime(tNow, m_tDummyMessageLastSent) > 1) 
		if (tNow - m_tDummyMessageLastSent > 1) // faster than the line above, and should be ok
		{
			const char* c = "keep_alive";
			int ret = SP_multicast(pimpl_->m_mailbox, FIFO_MESS, "keep_alive", 0, (int) strlen(c), c);
			if (ret < 0) {
				OnSpreadError(ret, "Dummy-send");
			}
			else {
				m_tDummyMessageLastSent = tNow;
			}
		}
	}

	return IsMailboxOk();
}

bool PostOffice::WaitForActivity(double maxSecondsToWait) const
{
	if (pimpl_->m_mailbox != NO_MAILBOX)
	{
		std::vector<SOCKET> v;
		v.push_back(pimpl_->m_mailbox);
		return pimpl_->m_signalingSelect.WaitForActivity(maxSecondsToWait, v);
	}
	else {
		sleep_minimal();
		return true;
	}
}

void PostOffice::Activity()
{
	pimpl_->m_signalingSelect.Activity();
}

}
