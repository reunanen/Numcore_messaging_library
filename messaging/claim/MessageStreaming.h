
//           Copyright 2007-2008 Juha Reunanen
//                     2008-2011 Numcore Ltd
//
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#ifndef CLAIM_MESSAGE_STREAMING_H
#define CLAIM_MESSAGE_STREAMING_H

#include <messaging/slaim/message.h>
#include <iostream>

namespace claim {

void WriteMessageToStream(std::ostream& output, const slaim::Message& msg);
bool ReadMessageFromStream(std::istream& input, slaim::Message& msg);

}

#endif // CLAIM_MESSAGE_STREAMING_H
