
//           Copyright 2007-2008 Juha Reunanen
//                     2008-2011 Numcore Ltd
//
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#ifdef WIN32
#pragma warning (disable: 4786)
#endif // WIN32

#include "MessageStreaming.h"
#include <vector>

namespace claim {

void WriteMessageToStream(std::ostream& output, const slaim::Message& msg)
{
	size_t len = msg.m_type.length();
	output.write((const char*) &len, sizeof(int));
	output.write(msg.m_type.data(), (std::streamsize) len);

	len = msg.m_text.length();
	output.write((const char*) &len, sizeof(int));
	output.write(msg.m_text.data(), (std::streamsize) len);
}

bool ReadMessageFromStream(std::istream& input, slaim::Message& msg)
{
	bool retval = false;
	int len = -1;

	input.read((char*) &len, sizeof(int));
	if (len > 0) {
		try {
			std::vector<char> buf(len);
			input.read(&buf[0], len);
			if (input.good()) {
				msg.m_type = std::string(&buf[0], len);
				
				len = -1;
				input.read((char*) &len, sizeof(int));
				if (len > 0) {
					buf.resize(len);
					input.read(&buf[0], len);
					msg.m_text = std::string(&buf[0], len);

					if (input.good()) {
						retval = true;
					}
				}
			}
		}
		catch (...) {
			retval = false;
		}
	}

	return retval;
}

}
