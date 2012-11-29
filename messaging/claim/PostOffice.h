
//           Copyright 2007-2008 Juha Reunanen
//                     2008-2011 Numcore Ltd
//
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#ifndef CLAIM_POSTOFFICE_H
#define CLAIM_POSTOFFICE_H

#include <messaging/slaim/postoffice.h>
#include <messaging/claim/PostOfficeInitializer.h>

//! Complex Library for Application-Independent Messaging, or -- between friends -- just <code>claim</code>.
/*! This library contains classes and functions that extend the methods that the 
	libraries <code>slaim</code>, <code>numsprew</code> and <code>numcfc</code> 
	offer, partially by combining the benefits due to some of these three.
*/
namespace claim {

//! A really thin wrapper for a shared_ptr to a slaim::PostOffice object.
class PostOffice : public slaim::PostOffice {
public:
	PostOffice();
	virtual ~PostOffice();

	//PostOffice(PostOfficeInitializer& initializer, const char* clientIdentifier = NULL) { Initialize(initializer, clientIdentifier); };

	//! Before Initialize is called, all other methods are going to throw.
	/*! \param initializer The object that supplies the necessary initialization parameters.
		\param clientIdentifier The maximum length of the identifier depends on the implementation.
	           However, the client programmer should not really have to worry about this; the 
			   identifier will surely be truncated, if necessary. To stay on the safe side however, 
			   it might be good to limit it to, say, 8 characters at most.
	*/
	void Initialize(PostOfficeInitializer& initializer, const char* clientIdentifier);
	
	//! A convenience method to wrap the declaration of the claim::IniFilePostOfficeInitializer object
	void Initialize(numcfc::IniFile& iniFile, const char* clientIdentifier = NULL);

	virtual void Subscribe(const slaim::MessageType& t);
	virtual void Unsubscribe(const slaim::MessageType& t);
	virtual bool Send(const slaim::Message& msg);
	virtual bool Receive(slaim::Message& msg, double maxSecondsToWait = 0);

	virtual void SetClientIdentifier(const std::string& clientIdentifier); // deprecated
	virtual std::string GetClientAddress() const;
	virtual const char* GetVersion() const;
	virtual std::string GetError();

private:
	// make the class non-copyable
    PostOffice(const PostOffice&);
    PostOffice& operator= (const PostOffice&);

	void CopyError();
	void CheckInitialized() const; // throws if not

	class Impl;
	Impl* pimpl_;
};

std::string PrettyPostOfficeVersion(const char* szInput);

}

#endif // CLAIM_POSTOFFICE_H
