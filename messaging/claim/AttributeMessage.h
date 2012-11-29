
//           Copyright 2007-2008 Juha Reunanen
//                     2008-2011 Numcore Ltd
//
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#ifndef CLAIM_ATTRIBUTE_MESSAGE_H
#define CLAIM_ATTRIBUTE_MESSAGE_H

#include <string>
#include <map>
#include <messaging/slaim/message.h>

//! Complex Library for Application-Independent Messaging, or -- between friends -- just <code>claim</code>.
/*! This library contains classes and functions that extend the methods that the 
	libraries <code>slaim</code>, <code>numsprew</code> and <code>numcfc</code> 
	offer, partially by combining the benefits due to some of these three.
*/
namespace claim {

/*! A class that makes it possible to easily extend slaim::Message objects to 
	contain simple name-value mappings.
*/
class AttributeMessage {
public:
	std::string m_body;
	std::string m_type;

	typedef std::map<std::string, std::string> Attributes;
	Attributes m_attributes;

	AttributeMessage();
	AttributeMessage(const slaim::Message& src);
	AttributeMessage& operator= (const slaim::Message& src);
	
	void ToRawMessage(slaim::Message& msg) const;
	slaim::Message GetRawMessage() const;

	operator slaim::Message () const;
};

}

#endif // CLAIM_ATTRIBUTE_MESSAGE_H
