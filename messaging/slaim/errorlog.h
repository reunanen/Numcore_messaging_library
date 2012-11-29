
//           Copyright 2007-2008 Juha Reunanen
//                     2008-2011 Numcore Ltd
//
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#ifndef SLAIM_ERRORLOG_H
#define SLAIM_ERRORLOG_H

#ifdef WIN32
#pragma warning(disable: 4786) // MSVC++: ignore the "identifier was truncated to '255' characters in the debug information" warning
#endif // WIN32

#include <list>
#include <string>

// todo: time stamping based on occurrence time!

namespace slaim {

class ErrorLog {
public:
	ErrorLog(unsigned int maxItems = 10) : m_maxItems(maxItems) {};

	virtual bool SetError(const std::string& strError) { 
		if (strError.empty()) {
			return false;
		}
		else if (strError == m_strLastError) {
			return false;
		}
		else if (m_errors.size() >= m_maxItems) {
			m_errors.pop_back();
			m_errors.push_back("...");
			return false; 
		}
		else { 
			m_errors.push_back(strError);
			return true;
		}
	}

	virtual std::string GetError() {
		std::string err;
		if (!m_errors.empty()) {
			err = m_errors.front();
			m_errors.pop_front();
		}
		return err;
	}

	bool HasError() {
		return !m_errors.empty();
	}

	void ClearLastError() {
		m_strLastError = "";
	}

private:
	unsigned int m_maxItems;
	std::list<std::string> m_errors;
	std::string m_strLastError;
};

}

#endif // SLAIM_ERRORLOG_H
