
//           Copyright 2007-2008 Juha Reunanen
//                     2008-2011 Numcore Ltd
//
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#ifndef SLAIM_BUFFER_ITEM_H
#define SLAIM_BUFFER_ITEM_H

#include <cstring> // for memcpy

namespace slaim {

//! In applications, please avoid using this class directly!
class BufferItem {
public:
	char* m_data;
	size_t m_length;

	BufferItem() { m_data = NULL; m_length = 0; }
	~BufferItem() { Delete(); }

	BufferItem(const char* data, size_t length) {
		m_data = new char[length];
		m_length = length;
		memcpy(m_data, data, length);
	}

	BufferItem(const std::string& str) {
		m_data = new char[str.length()];
		m_length = str.length();
		memcpy(m_data, str.data(), m_length);
	};

	void HijackFrom(BufferItem& item) {
		Delete();
		m_data = item.m_data;
		m_length = item.m_length;
		item.Forget();
	}

	void Forget() {
		m_data = NULL;
		m_length = 0;
	}

	void Delete() {
		if (m_data) {
			delete [] m_data;
			m_data = NULL;
			m_length = 0;
		}
	}
};

}

#endif // SLAIM_BUFFER_ITEM_H
