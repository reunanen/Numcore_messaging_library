
//           Copyright 2007-2008 Juha Reunanen
//                     2008-2011 Numcore Ltd
//
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

// if FASTER_IMPLEMENTATION is defined, itoa() is used instead of ostringstream
#ifdef WIN32
#define FASTER_IMPLEMENTATION 1
#endif // WIN32

#if FASTER_IMPLEMENTATION
#define _CRT_SECURE_NO_WARNINGS // no security warning for _itoa
#endif // FASTER_IMPLEMENTATION

#include "postoffice.h"
#include "buffer.h"

#ifdef WIN32
//#include <winsock.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#else
typedef int SOCKET;
#endif // WIN32

#include <stdexcept>
#include <assert.h>
#include <time.h>

#include <cstring>
#include <cstdlib>
#include <sstream>
#include <vector>

#ifdef _DEBUG
#include <valarray> // needed for log10
#endif // _DEBUG

namespace slaim {


// Retain here, in case this list will be useful later on.
/*
void PostOffice::OnSocketError(const char* szComment) {
	int e = WSAGetLastError();

	std::string err;

	switch (e) {
	default						: err = "Unrecognized socket error"; break;
	case 	WSAEINTR			: err = "Interrupted system call"; break;
	case 	WSAEBADF            : err = "Bad file number"; break;
	case 	10013               : err = "Permission denied"; break; // WSEACCES
	case 	WSAEFAULT           : err = "Bad address"; break;
	case 	WSAEINVAL           : err = "Invalid argument"; break;
	case 	WSAEMFILE           : err = "Too many open files/sockets"; break;
	case 	WSAEWOULDBLOCK      : err = "Operation would block"; break;
	case 	WSAEINPROGRESS      : err = "Operation now in progress"; break;
	case 	WSAEALREADY         : err = "Operation already in progress"; break;
	case 	WSAENOTSOCK         : err = "Socket operation on nonsocket"; break;
	case 	WSAEDESTADDRREQ     : err = "Destination address required"; break;
	case 	WSAEMSGSIZE         : err = "Message too long"; break;
	case 	WSAEPROTOTYPE       : err = "Protocol wrong type for socket"; break;
	case 	WSAENOPROTOOPT      : err = "Protocol not available/bad protocol option"; break;
	case 	WSAEPROTONOSUPPORT  : err = "Protocol not supported"; break;
	case 	WSAESOCKTNOSUPPORT  : err = "Socket type not supported"; break;
	case 	WSAEOPNOTSUPP       : err = "Operation not supported on socket"; break;
	case 	WSAEPFNOSUPPORT     : err = "Protocol family not supported"; break;
	case 	WSAEAFNOSUPPORT     : err = "Address family not supported by protocol family"; break;
	case 	WSAEADDRINUSE       : err = "Address already in use"; break;
	case 	WSAEADDRNOTAVAIL    : err = "Cannot assign requested address"; break;
	case 	WSAENETDOWN         : err = "Network is down"; break;
	case 	WSAENETUNREACH      : err = "Network is unreachable"; break;
	case 	WSAENETRESET        : err = "Network dropped connection on reset"; break;
	case 	WSAECONNABORTED     : err = "Software caused connection abort"; break;
	case 	WSAECONNRESET       : err = "Connection reset by peer"; break;
	case 	WSAENOBUFS          : err = "No buffer space available"; break;
	case 	WSAEISCONN          : err = "Socket is already connected"; break;
	case 	WSAENOTCONN         : err = "Socket is not connected"; break;
	case 	WSAESHUTDOWN        : err = "Cannot send after socket shutdown"; break;
	case 	WSAETOOMANYREFS     : err = "Too many references: cannot splice"; break;
	case 	WSAETIMEDOUT        : err = "Connection timed out"; break;
	case 	WSAECONNREFUSED     : err = "Connection refused"; break;
	case 	WSAELOOP            : err = "Too many levels of symbolic links"; break;
	case 	WSAENAMETOOLONG     : err = "File name too long"; break;
	case 	WSAEHOSTDOWN        : err = "Host is down"; break;
	case 	WSAEHOSTUNREACH     : err = "No route to host"; break;
	case 	WSAENOTEMPTY        : err = "Directory not empty"; break;
	case 	WSAEPROCLIM         : err = "Too many processes"; break;
	case 	WSAEUSERS           : err = "Too many users"; break;
	case 	WSAEDQUOT           : err = "Disc quota exceeded"; break;
	case 	WSAESTALE           : err = "Stale NFS file handle"; break;
	case 	WSAEREMOTE          : err = "Too many levels of remote in path"; break;
	case 	WSASYSNOTREADY      : err = "Network subsystem is unavailable"; break;
	case 	WSAVERNOTSUPPORTED  : err = "Winsock version not supported"; break;
	case 	WSANOTINITIALISED   : err = "Winsock not yet initialized"; break;
	case 	WSAEDISCON          : err = "Graceful disconnect in progress"; break;
	case 	WSAENOMORE          : err = "WSAENOMORE"; break;
	case 	WSAECANCELLED       : err = "WSAECANCELLED"; break;
	case 	WSAEINVALIDPROCTABLE: err = "WSAEINVALIDPROCTABLE"; break;
	case 	WSAEINVALIDPROVIDER : err = "WSAEINVALIDPROVIDER"; break;
	case 	WSAEPROVIDERFAILEDINIT: err = "WSAEPROVIDERFAILEDINIT"; break;
	case 	WSASYSCALLFAILURE   : err = "System call failure. (WS2)"; break;
	case 	WSASERVICE_NOT_FOUND: err = "WSASERVICE_NOT_FOUND"; break;
	case 	WSATYPE_NOT_FOUND   : err = "WSATYPE_NOT_FOUND"; break;
	case 	WSA_E_NO_MORE       : err = "WSA_E_NO_MORE"; break;
	case 	WSA_E_CANCELLED     : err = "WSA_E_CANCELLED"; break;
	case 	WSAEREFUSED         : err = "WSAEREFUSED"; break;
	case 	WSAHOST_NOT_FOUND   : err = "Host not found"; break;
	case 	WSATRY_AGAIN        : err = "Non-authoritative host not found"; break;
	case 	WSANO_RECOVERY      : err = "Non-recoverable error"; break;
	case 	WSANO_DATA          : err = "Valid name, no data record of requested type"; break;
	case 	WSA_NOT_ENOUGH_MEMORY: err = "Insufficient memory available"; break;
	case 	WSA_OPERATION_ABORTED: err = "Overlapped operation aborted"; break;
	case 	WSA_IO_INCOMPLETE   : err = "Overlapped I/O object not signalled"; break;
	case 	WSA_IO_PENDING      : err = "Overlapped I/O will complete later"; break;
	case 	WSA_INVALID_PARAMETER: err = "One or more parameters are invalid"; break;
	case 	WSA_INVALID_HANDLE  : err = "Event object handle not valid"; break;
	}

	std::ostringstream oss;
	oss << err << " (WSA Error " << e << ")";
	if (szComment) {
		oss << " - " << szComment;
	}
	oss << ".";

	SetError(oss.str());
}
*/

const MessageType& Message::GetType() const
{
	return m_type;
}
const std::string& Message::GetText() const
{
	return m_text;
}
void Message::SetType(const MessageType& type)
{
	if (type.find("\t") != std::string::npos) {
		throw std::runtime_error("Tabs are not allowed in the message type!");
	}
	m_type = type;
}
void Message::SetText(const std::string& text)
{
	m_text = text;
}
void Message::SetText(const char* p, size_t len)
{
	m_text.resize(len);
	if (len > 0) {
		size_t addressOffset = &m_text[len-1] - &m_text[0];
		if (addressOffset == len - 1) {
			memcpy(&m_text[0], p, len);
		}
		else {
			// should not happen; see, e.g., http://www.phwinfo.com/forum/comp-lang-cplus/329244-non-const-version-std-string-data.html
			m_text = std::string(p, p + len);
		}
	}
}
size_t Message::GetSize() const
{
	return m_type.length() + m_text.length();
}


Buffer::~Buffer()
{
	while (!m_data.empty()) {
		BufferItem& item = m_data.front();
		item.Delete();
		m_data.pop_front();
	}
}

bool Buffer::CanPush(unsigned int length) const
{
	bool r = false;
	if (m_data.size() < m_maxItems) {
		if (m_currentBytes + length < m_maxBytes) {			
			r = true;
		}
	}
	return r;
}

bool Buffer::Push(BufferItem& datum)
{
	bool r = false;
	if (m_data.size() < m_maxItems) {
		if (m_currentBytes + datum.m_length < m_maxBytes) {
			m_data.push_back(datum);
			m_currentBytes += datum.m_length;
			datum.Forget();
			r = true;
		}
	}
	return r;
}

bool Buffer::Pop(BufferItem& datum)
{
	bool r = false;
	if (!m_data.empty()) {
		datum = m_data.front();
		m_data.pop_front();
		m_currentBytes -= datum.m_length;
		r = true;
	}
	return r;
}

void Buffer::ForcePushFront(BufferItem& datum)
{
	m_data.push_front(datum);
	m_currentBytes += datum.m_length;
	datum.Forget();
}

void SerializeMessage(BufferItem& item, const Message& msg)
{
	// total size of contents in bytes
	size_t nContentsLength = msg.m_type.length() + msg.m_text.length() + 4;
	size_t nContentsLengthLength = 0; // to be set below...

#if !FASTER_IMPLEMENTATION
	// this is a bit slow...
	std::ostringstream ossContentsLength;
	ossContentsLength << nContentsLength;
	std::string strContentsLength = ossContentsLength.str();
	nContentsLengthLength = strContentsLength.length();
#else // !FASTER_IMPLEMENTATION
	// this is faster
	const int bufLen = 256;
	// make sure that even the largest value will fit in the buffer
#ifdef _DEBUG
	double maxN = static_cast<double>(static_cast<size_t>(-1));
	assert(bufLen - 1 > log10(maxN));
#endif // _DEBUG
	char pContentsLengthBuf[bufLen];
	_itoa(static_cast<int>(nContentsLength), pContentsLengthBuf, 10);
	nContentsLengthLength = strlen(pContentsLengthBuf);
#endif // FASTER_IMPLEMENTATION

	item.Delete();

	item.m_length = 3 + nContentsLengthLength + nContentsLength;
	item.m_data = new char[item.m_length];
	char* p = item.m_data;
	*p++ = '[';
#if !FASTER_IMPLEMENTATION
	memcpy(p, strContentsLength.data(), nContentsLengthLength);
#else // !FASTER_IMPLEMENTATION
	memcpy(p, pContentsLengthBuf, nContentsLengthLength);
#endif // FASTER_IMPLEMENTATION
	p += nContentsLengthLength;
	*p++ = ' ';
	*p++ = '(';
	memcpy(p, msg.m_type.data(), msg.m_type.length());
	p += msg.m_type.length();
	*p++ = ' ';
	memcpy(p, msg.m_text.data(), msg.m_text.length());
	p += msg.m_text.length();
	*p++ = ')';
	*p++ = '\n';
	*p++ = ']';

	assert(p == item.m_data + item.m_length);
}

bool ExtractSingleMessageFromBufferItem(BufferItem* bufferItem, Message& msg)
{
	if (!bufferItem) {
		return false;
	}

	BufferItem& dataStream = *bufferItem;
	char* pDataLengthEndPos = (char*) memchr(dataStream.m_data, ' ', dataStream.m_length);

	bool messageExtracted = false;

	if (pDataLengthEndPos != NULL) {
		unsigned int nDataLength = atoi(dataStream.m_data + 1);

		if (nDataLength < 5) {
			char* pEndPos = (char*) memchr(dataStream.m_data, ']', dataStream.m_length);
			if (pEndPos != NULL) {
				BufferItem temp(pEndPos + 1, dataStream.m_data + dataStream.m_length - pEndPos - 1);
				dataStream.HijackFrom(temp);
			}
			else {
				dataStream.Delete();
			}
		}
		else {
			char* pDataEndPos = pDataLengthEndPos + 2 + nDataLength;
			if (dataStream.m_data + dataStream.m_length >= pDataEndPos) {
				// whole message!

				// actual message
				char* pMessageTypeEndPos = (char*) memchr(pDataLengthEndPos + 2, ' ', dataStream.m_data + dataStream.m_length - pDataLengthEndPos - 2);

				if (pMessageTypeEndPos != NULL) {
					msg.m_type = std::string(pDataLengthEndPos + 2, pMessageTypeEndPos);

					// BEGIN: do essentially this:
					//msg.m_text = std::string(pMessageTypeEndPos + 1, pDataEndPos - 3);
					size_t dataLen = pDataEndPos - 3 - (pMessageTypeEndPos + 1);
					msg.SetText(pMessageTypeEndPos + 1, dataLen);
					// END

					if (*(pDataEndPos-1) == ']' && *(pDataEndPos-2) == '\n' && *(pDataEndPos-3) == ')') {
						messageExtracted = true;
					}
					else {
						// error...
					}
				}
				else {
					// error...
				}

				// remove it from dataStream
				if (dataStream.m_data + dataStream.m_length > pDataEndPos) {
					BufferItem temp(pDataEndPos, dataStream.m_data + dataStream.m_length - pDataEndPos);
					dataStream.HijackFrom(temp);
				}
				else {
					dataStream.Delete();
				}
			}
		}
	}

	return messageExtracted;
}

bool ExtractSingleMessage(Buffer* buffer, Message& msg) 
{
	if (!buffer) {
		return false;
	}

	bool messageReceived = false;

	BufferItem datum, prevData;
	// first try something simple
	if (buffer->Pop(datum)) {
		messageReceived = ExtractSingleMessageFromBufferItem(&datum, msg);
		if (messageReceived) {
			if (datum.m_length > 0) {
				buffer->ForcePushFront(datum);
			}
			return true;
		}
	}
	else {
		return false;
	}

	if (buffer->GetTotalBytes() == 0) {
		buffer->ForcePushFront(datum);
	}
	else {
		// not successful, and there's more data... prepare some memory for merging the segments
		size_t bufSize = buffer->GetTotalBytes() + datum.m_length;

		if (bufSize > 0) {
			prevData.m_data = new char[bufSize];
			prevData.m_length = datum.m_length;
			if (datum.m_length > 0) {
				memcpy(prevData.m_data, datum.m_data, datum.m_length);
				datum.Delete();		
			}

			while (buffer->Pop(datum)) {
				if (prevData.m_length + datum.m_length > bufSize) {
					assert(false);
				}
				memcpy(prevData.m_data + prevData.m_length, datum.m_data, datum.m_length);
				prevData.m_length += datum.m_length;
				datum.Delete();
			}

			messageReceived = ExtractSingleMessageFromBufferItem(&prevData, msg);

			if (prevData.m_length > 0) {
				buffer->ForcePushFront(prevData);
			}
		}
	}

	return messageReceived;
}

Message ConvertMessageListToSingleMessage(const MessageList& lst)
{
	Message msg;
	msg.m_type = "MessageList";
	size_t totalBytes = 0;

	BufferItem *items = new BufferItem[lst.size()];
	try {
		// serialize and count how many bytes are needed
		MessageList::const_iterator iMsg = lst.begin(), iMsgEnd = lst.end();
		size_t index = 0;
		for (; iMsg != iMsgEnd; ++iMsg, ++index) {
			BufferItem& item = items[index];
			SerializeMessage(item, *iMsg);
			totalBytes += item.m_length;
		}
		assert(index == lst.size());

		// reserve at once
		msg.m_text.resize(totalBytes);

		// then copy
		size_t lastIndex = lst.size();
		size_t pos = 0;
		for (index = 0; index < lastIndex; ++index) {
			BufferItem& item = items[index];
			memcpy(&msg.m_text[pos], item.m_data, item.m_length);
			pos += item.m_length;
			item.Delete();
		}
	}
	catch (...) {
		delete [] items;
		throw;
	}
	delete [] items;
	return msg;
}

void ConvertSingleMessageToMessageList(const Message& msg, MessageList& lst)
{
	BufferItem dataStream(msg.m_text);

	Message msgExtracted;
	while (ExtractSingleMessageFromBufferItem(&dataStream, msgExtracted)) {
		lst.push_back(msgExtracted);
	}
	dataStream.Delete();
}


}
