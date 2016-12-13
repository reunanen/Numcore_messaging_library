//              Copyright 2016 Juha Reunanen
//
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#ifndef NUMCFC_STRINGBUILDER_H
#define NUMCFC_STRINGBUILDER_H

#include <sstream>

namespace numcfc {

class StringBuilder {
public:
    template<typename T>
    StringBuilder& operator <<(const T& object) 
    {
        buffer << object;
        return *this;
    }

    operator std::string() const
    {
        return buffer.str();
    }

private:
    std::ostringstream buffer;
};

}

#endif // NUMCFC_STRINGBUILDER_H
