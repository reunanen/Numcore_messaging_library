
//           Copyright 2007-2008 Juha Reunanen
//                     2008-2011 Numcore Ltd
//
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#ifndef SLAIM_BUFFER_H
#define SLAIM_BUFFER_H

#ifdef WIN32
#pragma warning(disable: 4786) // MSVC++: ignore the "identifier was truncated to '255' characters in the debug information" warning
#endif // WIN32

#include <list>
#include <string>

#include "bufferitem.h"

namespace slaim {

//! A simple buffer implementation that can be used in producer-consumer type of communication.
/*! In applications however, please avoid using this class directly.
*/
class Buffer {
public:
	Buffer(unsigned int maxItems = 100, unsigned int maxBytes = 16*1024*1024) 
		: m_maxItems(maxItems)
		, m_maxBytes(maxBytes)
		, m_currentBytes(0) 
	{};

	~Buffer();

	bool IsEmpty() const { return m_data.empty(); }

	bool CanPush(unsigned int lenght) const;

	bool Push(BufferItem& datum);

	bool Pop(BufferItem& datum);

	//! Always succeeds.
	void ForcePushFront(BufferItem& datum);

	size_t GetTotalBytes() { return m_currentBytes; };

private:
	unsigned int m_maxItems;
	size_t m_maxBytes;
	size_t m_currentBytes;

	std::list<BufferItem> m_data;
};

}

#endif // SLAIM_BUFFER_H
