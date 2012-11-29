
//           Copyright 2007-2008 Juha Reunanen
//                     2008-2011 Numcore Ltd
//
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#ifndef SLAIM_MESSAGE_H
#define SLAIM_MESSAGE_H

#include <string>
#include <set>
#include <map>
#include <list>

namespace slaim {

typedef std::string MessageType;

//! A generic slaim message.
class Message
{
public:
	const MessageType& GetType() const;
	const std::string& GetText() const;

	void SetType(const MessageType& type);
	void SetText(const std::string& text);
	void SetText(const char* p, size_t len);

	size_t GetSize() const;

	MessageType m_type; // moving to getters and setters...:
	std::string m_text; // please don't write new code that would access these directly!
};

//! A list of slaim messages.
typedef std::list<Message> MessageList;

Message ConvertMessageListToSingleMessage(const MessageList& lst);

void ConvertSingleMessageToMessageList(const Message& msg, MessageList& lst);

}

#endif // SLAIM_MESSAGE_H
