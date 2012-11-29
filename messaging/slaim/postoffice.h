
//           Copyright 2007-2008 Juha Reunanen
//                     2008-2011 Numcore Ltd
//
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#ifndef SLAIM_POSTOFFICE_H
#define SLAIM_POSTOFFICE_H

#include <string>
#include <set>
#include <map>

#include "errorlog.h"
#include "message.h"

namespace slaim {

//! A generic post office interface.
/*! A key concept in any slaim system, post offices are used by applications to tell what kind of data 
	they would like to receive, and also to actually receive the data, plus to send messages to other
	clients.

	The clients should aim to call PostOffice::Receive() rather often, because the underlying 
	implementation typically wouldn't maintain a large buffer.
*/
class PostOffice 
: public ErrorLog 
{
public:
	//! This class was designed to support being subclassed.
	virtual ~PostOffice() {}

	//! Subscribe to messages of a certain type.
	/*! \param t Message type to subscribe to.
	*/
	virtual void Subscribe(const MessageType& t) = 0;

	//! Unsubscribe a message type.
	/*! \param t Message type to unsubscribe.
	*/
	virtual void Unsubscribe(const MessageType& t) = 0;

	//! Send a message.
	/*! \param msg The message to send.
		\return An indication whether the postoffice succeeded in delivering the message, or not.
				Depending on the implementation, the return value might not be very accurate. 
				This is because the post office might buffer the message to be sent in the near
				future, and return from Send() immediately. If this is the case, there is no way 
				to know	at return time whether the eventual send operation will be successful.
				On the other hand, in a multi-receiver environment, it might not even be that 
				well-defined when we should say that the sending was successful.
				(Does the message have to be delivered to all clients, or at least one? What if
				a client is just closing itself?)
	*/
	virtual bool Send(const Message& msg) = 0;

	//! Try to receive a message.
	/*! \param msg The received message is copied to this object.
		\param maxSecondsToWait The maximum time in seconds to wait for activity.
		\return If the return value is true, then a complete message was received.
	*/
	virtual bool Receive(Message& msg, double maxSecondsToWait = 0) = 0;

	//! Set a tag identifying the client. 
	/*! \param clientIdentifier The maximum length of the identifier depends on the implementation.
	           However, the client programmer should not really have to worry about this; the 
			   identifier will surely be truncated, if necessary. To stay on the safe side however, 
			   it might be good to limit it to, say, 8 characters at most.
			   THIS METHOD HAS BEEN DEPRECATED!
	*/
	virtual void SetClientIdentifier(const std::string& clientIdentifier) = 0;

	//! Get the address identifying the client. 
	virtual std::string GetClientAddress() const = 0;

	//! An arbitrary string to somehow identify the version that is being used.
	/*! \return A value -- somehow indicating the version of the post office -- that can be logged, 
				shown to the user, etc.
	*/
	virtual const char* GetVersion() const = 0;
};

}

#endif // SLAIM_POSTOFFICE_H
