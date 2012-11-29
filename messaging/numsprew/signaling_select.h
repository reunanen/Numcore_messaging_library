
//           Copyright 2008      Juha Reunanen
//                     2008-2011 Numcore Ltd
//
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#ifndef NUMSPREW_SIGNALING_SELECT_H
#define NUMSPREW_SIGNALING_SELECT_H

// signaling select()
// see: http://stackoverflow.com/questions/384391/how-to-signal-select-to-return-immediately

#ifdef WIN32
#define USE_NONBOUND_SIGNALING_SOCKET
#else // WIN32
#define USE_SELF_PIPE_TRICK
#endif // WIN32

#include <vector>

#ifdef WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <winbase.h>
#else
typedef int SOCKET;
#endif

namespace numsprew {

class SignalingSelect
{
public:
	SignalingSelect();
	~SignalingSelect();

	bool WaitForActivity(double maxSecondsToWait, std::vector<SOCKET>& socketsToWaitFor);
	void Activity();

private:
	int GetLastError();

#ifdef USE_NONBOUND_SIGNALING_SOCKET
	void RecreateSignalingSocket();
	SOCKET m_signalNonboundSocket;
#endif // USE_NONBOUND_SIGNALING_SOCKET

#ifdef USE_SELF_PIPE_TRICK
	int m_selfPipe[2];
#endif // USE_NONBOUND_SIGNALING_SOCKET
};

}

#endif // NUMSPREW_SIGNALING_SELECT_H
