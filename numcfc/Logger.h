
//           Copyright 2007-2008 Juha Reunanen
//                     2008-2011 Numcore Ltd
//
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#ifndef NUMCFC_LOGGER_H
#define NUMCFC_LOGGER_H

#include <string>

namespace numcfc {

//! A generic logger object that implements simple rotation.
class Logger  
{
public:
	//! The constructor
	/*!
		\param szLogName If NULL, a default name shall be generated. If non-NULL, don't add a suffix, because the function will add ".txt".
		\param maxSizeMB The maximum file size. The total log information available will be between maxSizeMB and 2 * maxSizeMB.
	*/
	Logger(const char* szLogName = NULL, double maxSizeMB = 1.0);

	virtual ~Logger();

	//! Log a line of text, optionally with source code location information.
	/*!
		\param str The text to log.
		\param szFile Use the __FILE__ macro to automatically log the source file.
		\param nLine Use the __LINE__ macro to automatically log the line number in the source file.
	*/
	bool Log(const std::string& str, const char* szFile = (const char*) NULL, unsigned int nLine = 0);

	//! If set, all log information is echoed to std::cout
	void SetEcho(bool echo = true);

	//! Log a line of text without explicitly constructing a Logger object.
	static bool LogAndEcho(const std::string& str, const char* szLogName = NULL, const char* szFile = (const char*) NULL, unsigned int nLine = 0);
	static bool LogNoEcho(const std::string& str, const char* szLogName = NULL, const char* szFile = (const char*) NULL, unsigned int nLine = 0);

private:
	bool m_echo;
	bool m_writeTimestamp;
	bool m_writeEndline;
	size_t m_currentSize, m_maxSize;
	std::string m_strFilename, m_strFilenamePrevious;
};

}

#endif // NUMCFC_LOGGER_H