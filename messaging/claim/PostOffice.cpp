
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

#include <memory>

#include <deque>
#include <mutex>
#include <sstream>
#include <thread>
#include <assert.h>

namespace claim {

std::shared_ptr<slaim::PostOffice> CreatePostOffice(PostOfficeInitializer& initializer, const char* clientIdentifier) {
	const std::string host = initializer.GetMessagingServerHost();
	const int port = initializer.GetMessagingServerPort();
	const std::string username = initializer.GetMessagingServerUsername();
	const std::string password = initializer.GetMessagingServerPassword();
    const std::string vhost = initializer.GetMessagingServerVirtualHost();

	std::ostringstream oss;
	if (!username.empty() && !password.empty()) {
		oss << username << ":" << password << "@";
	}
	if (host.empty()) {
		oss << "localhost";
	}
	else {
		oss << host;
	}
	oss << ":" << port;

    if (!vhost.empty()) {
        oss << "/" << vhost;
    }

	std::string connectInfo = oss.str();

	auto postOffice = std::make_shared<numrabw::PostOffice>(connectInfo, clientIdentifier);
	postOffice->ReadSettings(initializer);

	return postOffice;
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
	return pimpl_->postOffice->Receive(msg, maxSecondsToWait); 
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
    return pimpl_->postOffice->GetError();
}

}
