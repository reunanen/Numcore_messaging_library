
//           Copyright 2007-2008 Juha Reunanen
//                     2008-2011 Numcore Ltd
//                     2016      Juha Reunanen
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
	//! Can be overridden to avoid unnecessary sleeping and polling.
	/*! NB: May be called from the registered worker thread only. */
	virtual bool WaitForActivity(double maxSecondsToWait) = 0;

	//! Used to wake up the worker thread currently in WaitForActivity().
	/*! NB: May be called from any thread. */
	virtual void Activity() = 0;

#ifdef _DEBUG
	virtual void RegisterWorkerThread() = 0;
#endif
};

}

#endif // SLAIM_POSTOFFICE_EXTENDED_H
