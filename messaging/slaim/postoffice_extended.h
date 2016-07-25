
//           Copyright 2007-2008 Juha Reunanen
//                     2008-2011 Numcore Ltd
//
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#ifndef SLAIM_POSTOFFICE_EXTENDED_H
#define SLAIM_POSTOFFICE_EXTENDED_H

#include <string>
#include <set>
#include <map>

#include "postoffice.h"

namespace slaim {

//! An extended post office that allows some useful functions.
/*! Used at least by claim::BufferedPostOffice, and inherited by
    num0w::PostOffice.
    Probably not to be included directly by any applications. */
class ExtendedPostOffice 
: public PostOffice
{
public:
	//! Can be overridden to avoid unnecessary sleeping and polling
	virtual bool WaitForActivity(double maxSecondsToWait) const { return true; }

	//! Used to wake up anyone currently in WaitForActivity()
	virtual void Activity() {}
};

}

#endif // SLAIM_POSTOFFICE_EXTENDED_H
