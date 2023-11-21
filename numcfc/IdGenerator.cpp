
//           Copyright 2007-2008 Juha Reunanen
//                     2008-2011 Numcore Ltd
//                     2012-2023 Tomaattinen Ltd
//
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#include <numcfc/Time.h>
#include <numcfc/IdGenerator.h>

#include <time.h>
#include <string.h>

#include <sstream>
#include <cstdlib>

// for GetHostname()
#ifdef _WIN32
#include <winsock2.h>
#else // _WIN32
#include <netinet/in.h>
#include <ifaddrs.h>
#include <arpa/inet.h>
#endif // _WIN32

// for GetWorkingDirectory()
#ifdef WIN32
#include <direct.h>
#else
#include <unistd.h>
#define _getcwd(a,b) getcwd(a,b)
#define strcpy_s(a,b) strcpy(a,b)
#endif

namespace numcfc {

IdGenerator::IdGenerator()
{
	srand((unsigned int) (time(NULL) + clock()));

#ifdef WIN32
    WSAData wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);
#endif // WIN32
}

std::string IdGenerator::GenerateId() const
{
	std::ostringstream oss;
	Time t;
	t.InitCurrentUniversal();
	unsigned int r = (unsigned int) rand();
	oss << t.ToExtendedISO() << "/" << GetHostname() << "/" << r;
	return oss.str();
}

std::string GetHostname()
{
	char szHostname[1024];
	if (gethostname(szHostname, 1023) == 0) {
		return szHostname;
	}
	else {
		std::ostringstream oss;
#ifdef WIN32
		oss << "error_" << GetLastError();
#else // WIN32
		oss << "error_" << errno;
#endif // WIN32
		return oss.str();
	}
}

std::vector<std::string> GetIpAddresses(const std::string& hostname)
{
    std::vector<std::string> addresses;
#ifdef WIN32
    const auto hostinfo = gethostbyname(hostname.c_str());
    if (hostinfo != nullptr) {
        int i = 0;
        while (hostinfo->h_addr_list[i]) {
            const char* ip = inet_ntoa(*(struct in_addr *) hostinfo->h_addr_list[i]);
            addresses.push_back(ip);
            ++i;
        }
    }
#else
    // adapted from: https://stackoverflow.com/a/265978
    struct ifaddrs* ifAddrs = nullptr;
    getifaddrs(&ifAddrs);
    for (const auto* ifa = ifAddrs; ifa != nullptr; ifa = ifa->ifa_next) {
        if (ifa->ifa_addr) {
            if (ifa->ifa_addr->sa_family == AF_INET) {
                const auto* temp = &((struct sockaddr_in*) ifa->ifa_addr)->sin_addr;
                char addressBuffer[INET_ADDRSTRLEN + 1];
                inet_ntop(AF_INET, temp, addressBuffer, INET_ADDRSTRLEN);
                std::string address(addressBuffer);
                if (address != "127.0.0.1") {
                    addresses.push_back(address);
                }
            }
            else if (ifa->ifa_addr->sa_family == AF_INET6) {
                const auto* temp = &((struct sockaddr_in6*) ifa->ifa_addr)->sin6_addr;
                char addressBuffer[INET6_ADDRSTRLEN + 1];
                inet_ntop(AF_INET6, temp, addressBuffer, INET6_ADDRSTRLEN);
                std::string address(addressBuffer);
                if (address != "::1") {
                    addresses.push_back(addressBuffer);
                }
            }
        }
    }
    if (ifAddrs) {
        freeifaddrs(ifAddrs);
    }
#endif
    return addresses;
}

std::string GetWorkingDirectory()
{
	char buf[1024];
	if (_getcwd(buf, 1023) == NULL) {
		strcpy_s(buf, "getcwd() returned NULL!");
	}
	return buf;
}

std::string GenerateId(const std::string& hostname, const std::vector<std::string>& ipAddresses, const std::string& workingDirectory)
{
    std::string id = "hostname:" + hostname;
    id += ",ipAddresses:[";
    bool firstIpAddress = true;
    for (const auto& ipAddress : ipAddresses) {
        if (firstIpAddress) {
            firstIpAddress = false;
        }
        else {
            id += ",";
        }
        id += ipAddress;
    }
    id += "]";
    id += ",workingDirectory:" + workingDirectory;
    return id;
}

}
