
//           Copyright 2007-2008 Juha Reunanen
//                     2008-2011 Numcore Ltd
//
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#include "PostOfficeInitializer.h"

namespace claim {

std::string DefaultPostOfficeInitializer::GetMessagingServerHost()
{ 
#ifdef WIN32
	return "localhost";
#else // WIN32
	return "";
#endif // WIN32
}

int DefaultPostOfficeInitializer::GetMessagingServerPort()
{
	return 5672;
}

std::string DefaultPostOfficeInitializer::GetMessagingServerUsername()
{
	return "";
}

std::string DefaultPostOfficeInitializer::GetMessagingServerPassword()
{
	return "";
}

size_t DefaultPostOfficeInitializer::GetReceiveBufferMaxItemCount()
{
	return 262144;
}

double DefaultPostOfficeInitializer::GetReceiveBufferMaxMegabytes()
{
	return 256;
}

size_t DefaultPostOfficeInitializer::GetSendBufferMaxItemCount()
{
	return 262144;
}

double DefaultPostOfficeInitializer::GetSendBufferMaxMegabytes()
{
	return 256;
}

IniFilePostOfficeInitializer::IniFilePostOfficeInitializer(numcfc::IniFile& iniFile)
: iniFile(iniFile)
{ 
}

std::string IniFilePostOfficeInitializer::GetMessagingServerHost()
{
#ifdef WIN32
	const char* defaultHost = "localhost";
#else // WIN32
	const char* defaultHost = "";
#endif // WIN32

	std::string host = iniFile.GetSetValue("MessagingServer", "Host", defaultHost, "IP address or network hostname of the messaging server.");
	return host;
}

int IniFilePostOfficeInitializer::GetMessagingServerPort()
{
	int port = static_cast<int>(iniFile.GetSetValue("MessagingServer", "Port", 5672, "The port that the RabbitMQ server listens to (default is 5672)."));
	return port;
}

std::string IniFilePostOfficeInitializer::GetMessagingServerUsername()
{
	return iniFile.GetSetValue("MessagingServer", "Username", "guest", "The RabbitMQ username.");
}

std::string IniFilePostOfficeInitializer::GetMessagingServerPassword()
{
	return iniFile.GetSetValue("MessagingServer", "Password", "guest", "The RabbitMQ user's password.");
}

size_t IniFilePostOfficeInitializer::GetReceiveBufferMaxItemCount()
{
	size_t recvBufferMaxItemCount = static_cast<size_t>(iniFile.GetSetValue("PostOffice", "ReceiveBufferMaxItemCount", 262144, "Maximum item count for the receiving buffer."));
	return recvBufferMaxItemCount;
}

double IniFilePostOfficeInitializer::GetReceiveBufferMaxMegabytes()
{
	double recvBufferMaxMegabytes = iniFile.GetSetValue("PostOffice", "ReceiveBufferMaxMegabytes", 256, "Maximum size in megabytes for the receiving buffer.");
	return recvBufferMaxMegabytes;
}

size_t IniFilePostOfficeInitializer::GetSendBufferMaxItemCount()
{
	size_t sendBufferMaxItemCount = static_cast<size_t>(iniFile.GetSetValue("PostOffice", "SendBufferMaxItemCount", 262144, "Maximum item count for the send buffer."));
	return sendBufferMaxItemCount;
}

double IniFilePostOfficeInitializer::GetSendBufferMaxMegabytes()
{
	double sendBufferMaxMegabytes = iniFile.GetSetValue("PostOffice", "SendBufferMaxMegabytes", 256, "Maximum size in megabytes for the send buffer.");
	return sendBufferMaxMegabytes;
}

}
