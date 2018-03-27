
//           Copyright 2018 Juha Reunanen
//
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#ifndef NUMRABW_POSTOFFICE_H
#define NUMRABW_POSTOFFICE_H

#include <string>
#include <set>
#include <map>

#include "../slaim/postoffice.h"
#include "../claim/PostOfficeInitializer.h"

namespace numrabw {

//! The numrabw post office implementation that conforms to the generic slaim::PostOffice interface.
class PostOffice 
: public slaim::PostOffice
{
public:
	PostOffice(const std::string& connectString, const char* clientIdentifier);
	virtual ~PostOffice();

    virtual void Subscribe(const slaim::MessageType& t) override;
    virtual void Unsubscribe(const slaim::MessageType& t) override;

    virtual bool Send(const slaim::Message& msg) override;

	// If the return value is true, then a complete message was received.
	virtual bool Receive(slaim::Message& msg, double maxSecondsToWait = 0) override;

    bool IsOk() const; // probably not really needed

    virtual const char* GetVersion() const override;

#if 0
	//* NB: May be called from the registered worker thread only.
	virtual bool WaitForActivity(double maxSecondsToWait);
#endif

	//* May be called from any thread.
	virtual void Activity();

    virtual std::string GetError() override;

    virtual std::string GetClientAddress() const override;

    void ReadSettings(claim::PostOfficeInitializer& initializer);

private:
	const std::string connectString;

	class Pimpl;
	Pimpl* pimpl_;
};

}

#endif // NUMRABW_POSTOFFICE_H
