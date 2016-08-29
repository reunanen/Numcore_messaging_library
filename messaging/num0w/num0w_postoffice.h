
//           Copyright 2016 Juha Reunanen
//
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#ifndef NUM0W_POSTOFFICE_H
#define NUM0W_POSTOFFICE_H

#include <string>
#include <set>
#include <map>

#include "../slaim/postoffice.h"
#include "../slaim/postoffice_extended.h"

namespace num0w {

//! The num0w post office implementation that conforms to the generic slaim::PostOffice interface.
/*! Note that this class is *not* necessarily thread-safe, meaning that you should
    not call Send() and Receive() simultaneously from different threads.
    Rather, it's been designed to be used as follows:
    <code>
        while (true) {
#ifdef _DEBUG
            po.RegisterWorkerThread();
#endif
            slaim::Message msg;
            bool didSomething = false;
            while (po.Receive(msg)) {
                // do stuff
                didSomething = true;
            }
            while (GetNextMessageToSend(msg)) {
                po.Send(msg);
                didSomething = true;
            }
            if (!didSomething) {
                // either: numcfc::SleepMinimal()
                // or: po.WaitForActivity()
            }
        }
    </code>

    Implementing thread-safety would not be a big deal at all; just add the 
    corresponding mutexes. However, currently there's no need for it, so why 
    add the potential for the performance bottleneck?
*/
class PostOffice 
: public slaim::ExtendedPostOffice 
{
public:
	PostOffice(const std::string& connectString, const char* clientIdentifier);
	virtual ~PostOffice();

	//virtual void SetConnectInfo(const std::string& connectString);
	virtual void SetClientIdentifier(const std::string& clientIdentifier); // deprecated
	virtual std::string GetClientAddress() const;

	virtual void Subscribe(const slaim::MessageType& t);
	virtual void Unsubscribe(const slaim::MessageType& t);

	virtual bool Send(const slaim::Message& msg);

	// If the return value is true, then a complete message was received.
	virtual bool Receive(slaim::Message& msg, double maxSecondsToWait = 0);

	virtual const char* GetVersion() const;

	bool IsMailboxOk() const;

	//* NB: May be called from the registered worker thread only.
	virtual bool WaitForActivity(double maxSecondsToWait);

	//* May be called from any thread.
	virtual void Activity();

	virtual void RegisterWorkerThread();

private:
	void Register();
	bool CheckConnection();
	void RegularOperations();

	std::set<slaim::MessageType> m_mySubscriptions;

	std::list< std::pair<slaim::MessageType, bool> > m_myPendingSubscriptions;

	std::string m_connectString;
	std::string m_clientIdentifier;

	char* m_pReceiveBuffer;
	unsigned int m_nReceiveBufferSize;

	std::map<std::string, slaim::Message> m_multiMessagesBeingReceived;

	time_t m_tDummyMessageLastSent;

	class Pimpl;
	Pimpl* pimpl_;
};

}

#endif // NUM0W_POSTOFFICE_H
