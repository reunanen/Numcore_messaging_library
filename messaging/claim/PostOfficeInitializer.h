
//           Copyright 2007-2008 Juha Reunanen
//                     2008-2011 Numcore Ltd
//
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#ifndef CLAIM_POSTOFFICE_INITIALIZER_H
#define CLAIM_POSTOFFICE_INITIALIZER_H

#include <numcfc/IniFile.h>

namespace claim {

class PostOfficeInitializer
{
public:
	virtual std::string GetMessagingServerHost() = 0;
	virtual int GetMessagingServerPort() = 0;
	virtual std::string GetMessagingServerUsername() = 0;
	virtual std::string GetMessagingServerPassword() = 0;

	virtual size_t GetReceiveBufferMaxItemCount() = 0;
	virtual double GetReceiveBufferMaxMegabytes() = 0;
	virtual size_t GetSendBufferMaxItemCount() = 0;
	virtual double GetSendBufferMaxMegabytes() = 0;	
};

class DefaultPostOfficeInitializer : public PostOfficeInitializer
{
public:
	virtual std::string GetMessagingServerHost() override;
	virtual int GetMessagingServerPort() override;
	virtual std::string GetMessagingServerUsername() override;
	virtual std::string GetMessagingServerPassword() override;

	virtual size_t GetReceiveBufferMaxItemCount() override;
	virtual double GetReceiveBufferMaxMegabytes() override;
	virtual size_t GetSendBufferMaxItemCount() override;
	virtual double GetSendBufferMaxMegabytes() override;
};

class IniFilePostOfficeInitializer : public PostOfficeInitializer
{
public:
	IniFilePostOfficeInitializer(numcfc::IniFile& iniFile);

	virtual std::string GetMessagingServerHost() override;
	virtual int GetMessagingServerPort() override;
	virtual std::string GetMessagingServerUsername() override;
	virtual std::string GetMessagingServerPassword() override;

	virtual size_t GetReceiveBufferMaxItemCount() override;
	virtual double GetReceiveBufferMaxMegabytes() override;
	virtual size_t GetSendBufferMaxItemCount() override;
	virtual double GetSendBufferMaxMegabytes() override;

private:
	numcfc::IniFile& iniFile;
};

}

#endif // CLAIM_POSTOFFICE_INITIALIZER_H
