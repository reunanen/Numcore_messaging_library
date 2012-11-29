
//           Copyright 2008      Juha Reunanen
//                     2008-2011 Numcore Ltd
//
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#include "signaling_select.h"
#ifdef WIN32
#else // WIN32
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h> 
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#endif // WIN32

#include <stdio.h>
#include <assert.h>

namespace numsprew {

SignalingSelect::SignalingSelect()
{
#ifdef USE_NONBOUND_SIGNALING_SOCKET
	m_signalNonboundSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (m_signalNonboundSocket == -1) {
		//SetError("Unable to create the signaling socket!");
	}
#endif // USE_NONBOUND_SIGNALING_SOCKET
#ifdef USE_SELF_PIPE_TRICK
	int result = pipe(m_selfPipe);
	if (result < 0) {
		int error = GetLastError();
	}
	int flags = fcntl(m_selfPipe[1], F_GETFL, 0);
	fcntl(m_selfPipe[1], F_SETFL, flags | O_NONBLOCK);
#endif // USE_SELF_PIPE_TRICK
}

SignalingSelect::~SignalingSelect() {
#ifdef USE_NONBOUND_SIGNALING_SOCKET
	if (m_signalNonboundSocket != -1) {
		closesocket(m_signalNonboundSocket);
		m_signalNonboundSocket = -1;
	}
#endif // USE_NONBOUND_SIGNALING_SOCKET

#ifdef USE_SELF_PIPE_TRICK
	close(m_selfPipe[0]);
	close(m_selfPipe[1]);
#endif // USE_NONBOUND_SIGNALING_SOCKET
}

#ifdef USE_NONBOUND_SIGNALING_SOCKET
void SignalingSelect::RecreateSignalingSocket()
{
	closesocket(m_signalNonboundSocket); // probably already closed by Activity(), but this shouldn't do harm either
	m_signalNonboundSocket = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
}
#endif // USE_NONBOUND_SIGNALING_SOCKET

bool SignalingSelect::WaitForActivity(double maxSecondsToWait, std::vector<SOCKET>& socketsToWaitFor)
{
	if (
#ifdef USE_NONBOUND_SIGNALING_SOCKET
		m_signalNonboundSocket != -1
#else // USE_NONBOUND_SIGNALING_SOCKET
		true
#endif // USE_NONBOUND_SIGNALING_SOCKET
		) 
	{

#ifdef USE_NONBOUND_SIGNALING_SOCKET
		{
			// [1] check the condition of the non-bound signaling socket
			fd_set rfds;
			FD_ZERO(&rfds);

			FD_SET(m_signalNonboundSocket, &rfds);
			size_t nfds = m_signalNonboundSocket + 1;

			struct timeval tv;
			tv.tv_sec = tv.tv_usec = 0;
			int retval = select(static_cast<int>(nfds), &rfds, NULL, &rfds, &tv);
			if (retval > 0) {
				// shouldn't normally happen (?), because the select() timeout is zero
				RecreateSignalingSocket();
				return true;
			}
			else if (retval == -1) {
				int err = GetLastError();
				{
					// the non-bound signaling socket was closed by an Activity() call in between WaitForActivity() calls
					RecreateSignalingSocket();
					return true;
				}
			}
		}
#endif

		fd_set rfds, efds;
		SOCKET nfds = 0;

		// watch mailbox to see when it has input
		// (alternatively, can be woken up by calling Activity())
		FD_ZERO(&rfds);
		FD_ZERO(&efds);

#ifdef USE_NONBOUND_SIGNALING_SOCKET
		SOCKET fdActivity = m_signalNonboundSocket;
#endif // USE_NONBOUND_SIGNALING_SOCKET
#ifdef USE_SELF_PIPE_TRICK
		int fdActivity = m_selfPipe[0];
#endif // USE_SELF_PIPE_TRICK

		FD_SET(fdActivity, &rfds);
		FD_SET(fdActivity, &efds);
		nfds = fdActivity + 1;

		for (std::vector<SOCKET>::const_iterator i = socketsToWaitFor.begin(); i != socketsToWaitFor.end(); ++i) {
			SOCKET s = *i;
			FD_SET(s, &rfds);
			FD_SET(s, &efds);
			if (s + 1 > nfds) {
				nfds = s + 1;
			}
		}

		struct timeval tv;
		tv.tv_sec = static_cast<long>(maxSecondsToWait);
		tv.tv_usec = static_cast<long>((maxSecondsToWait - tv.tv_sec) * 1000000);

		int retval = select(static_cast<int>(nfds), &rfds, NULL, &efds, &tv);

		if (retval > 0) {
			if (FD_ISSET(fdActivity, &rfds)) {
#ifdef USE_NONBOUND_SIGNALING_SOCKET
				// the non-bound signaling socket was closed by Activity() while we were in select()
				RecreateSignalingSocket();
#endif // USE_NONBOUND_SIGNALING_SOCKET

#ifdef USE_SELF_PIPE_TRICK
				// the pipe was written to
				char a = '\0';
				int bytesRead = read(m_selfPipe[0], &a, 1);
				assert(bytesRead == 1);
				assert(a == 'a');
#endif // USE_SELF_PIPE_TRICK
			}

			return true;
		}
		else if (retval == 0) {
			// timeout occurred
			return false;
		}
		else {
#ifdef USE_NONBOUND_SIGNALING_SOCKET
			// the non-bound signaling socket was closed by calling Activity() while we were in this function (between [1] and the select() call)
			int err = GetLastError();
			RecreateSignalingSocket();
			return true;
#endif // USE_NONBOUND_SIGNALING_SOCKET

#ifdef USE_SELF_PIPE_TRICK
			return true;
#endif // USE_SELF_PIPE_TRICK
		}
	}
	else {
		return true;
	}
}

void SignalingSelect::Activity()
{
	// simulate activity in order to make it possible for a 
	// thread in WaitForActivity to immediately wake up
#ifdef USE_NONBOUND_SIGNALING_SOCKET
	closesocket(m_signalNonboundSocket);
#endif // USE_NONBOUND_SIGNALING_SOCKET

#ifdef USE_SELF_PIPE_TRICK
	char a = 'a';
	int result = write(m_selfPipe[1], &a, 1);
#endif // USE_SELF_PIPE_TRICK
}

int SignalingSelect::GetLastError()
{
#ifdef WIN32
	int err = WSAGetLastError();
#else // WIN32
	int err = errno;
#endif // WIN32
	return err;
}

}
