
//           Copyright 2007-2008 Juha Reunanen
//                     2008-2011 Numcore Ltd
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
#include <errno.h>
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

std::string GetWorkingDirectory()
{
	char buf[1024];
	if (_getcwd(buf, 1023) == NULL) {
		strcpy_s(buf, "getcwd() returned NULL!");
	}
	return buf;
}

}
