
//           Copyright 2007-2008 Juha Reunanen
//                     2008-2011 Numcore Ltd
//
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#ifndef NUMCFC_ID_GENERATOR_H
#define NUMCFC_ID_GENERATOR_H

#include <string>
#include <vector>

namespace numcfc
{

std::string GetHostname();
std::vector<std::string> GetIpAddresses(const std::string& hostname = GetHostname());
std::string GetWorkingDirectory();

std::string GenerateId(const std::string& hostname, const std::vector<std::string>& ipAddresses, const std::string& workingDirectory);

class IdGenerator {
public:
	IdGenerator();
	std::string GenerateId() const;
};

}

#endif // __NUMCFC_ID_GENERATOR_H__
