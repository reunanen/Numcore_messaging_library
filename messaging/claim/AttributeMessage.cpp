
//           Copyright 2007-2008 Juha Reunanen
//                     2008-2011 Numcore Ltd
//
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#ifdef WIN32
#pragma warning (disable: 4786)
#endif // WIN32

#include "AttributeMessage.h"

namespace claim {

AttributeMessage::AttributeMessage()
{
}

AttributeMessage::AttributeMessage(const slaim::Message& src)
{
	*this = src;
}

AttributeMessage& AttributeMessage::operator= (const slaim::Message& src)
{
	slaim::MessageList lst;
	slaim::ConvertSingleMessageToMessageList(src, lst);

	if (!lst.empty()) {
		m_attributes.clear();
	}

	m_type = src.GetType();

	slaim::MessageList::const_iterator iter = lst.begin(), iterEnd = lst.end();
	for (; iter != iterEnd; ++iter) {
		const slaim::Message& msg = *iter;
		if (msg.m_type == "m_body") {
			m_body = msg.m_text;
		}
		else {
			m_attributes[msg.m_type] = msg.m_text;
		}
	}
	return *this;
}
	
void AttributeMessage::ToRawMessage(slaim::Message& msg) const
{
	slaim::MessageList lst;
	slaim::Message temp;
	temp.m_type = "m_body";
	lst.push_back(temp); // push first and *then* set body, just to avoid making an unnecessary copy when the body is huge
	lst.back().m_text = m_body;
	for (Attributes::const_iterator i = m_attributes.begin(); i != m_attributes.end(); i++) {
		temp.m_type = i->first;
		temp.m_text = i->second;
		lst.push_back(temp);
	}
	msg = slaim::ConvertMessageListToSingleMessage(lst);
	msg.m_type = m_type;
}

slaim::Message AttributeMessage::GetRawMessage() const
{
	slaim::Message msg;
	ToRawMessage(msg);
	return msg;
}

AttributeMessage::operator slaim::Message () const
{
	return GetRawMessage();
}

}
