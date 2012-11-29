
//           Copyright 2007-2008 Juha Reunanen
//                     2008-2011 Numcore Ltd
//
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#include "Logger.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <sys/types.h>
#include <sys/stat.h>
#include "Time.h"

namespace numcfc {

Logger::Logger(const char* szLogName /* = NULL */, double maxSizeMB /* = 1.0 */)
{
	if (szLogName != NULL) {
		m_strFilename = szLogName;
	}
	else {
		m_strFilename = "log";
	}
	m_strFilenamePrevious = m_strFilename + "_old.txt";
	m_strFilename += ".txt";

	m_maxSize = (unsigned int) (maxSizeMB * 1024 * 1024);

#ifdef WIN32
	struct _stat s;
	bool bStatOk = (_stat(m_strFilename.c_str(), &s) == 0);
#else // WIN32
	struct stat s;
	bool bStatOk = (stat(m_strFilename.c_str(), &s) == 0);
#endif // WIN32

	if (bStatOk) {
		m_currentSize = s.st_size;
	}
	else {
		m_currentSize = 0;
	}

	m_writeTimestamp = true;
	m_writeEndline = true;
	m_echo = false;
}

Logger::~Logger()
{

}

void Logger::SetEcho(bool echo)
{
	m_echo = echo;
}

bool Logger::Log(const std::string &str, const char* szFile /* = (const char*) NULL */, unsigned int nLine /* = 0 */)
{
	std::ostringstream oss;
	if (m_writeTimestamp) {
		Time t(Time::Local);
		oss << t.ToExtendedISO() << ": ";
	}

	oss << str;

	if (szFile != NULL || nLine != 0) {
		oss << " [";
		if (szFile != NULL) {
			oss << szFile;
		}
		if (nLine != 0) {
			oss << ", " << nLine;
		}
		oss << "]";
	}

	if (m_writeEndline) {
		oss << std::endl;
	}

	std::string lineToLog = oss.str();

	if (m_currentSize + lineToLog.length() > m_maxSize) {
		remove(m_strFilenamePrevious.c_str());
		rename(m_strFilename.c_str(), m_strFilenamePrevious.c_str());
		if (m_echo) {
			std::cout << "[New log length would be " << m_currentSize << " + " << lineToLog.length() << " when max size is " << m_maxSize << ": rolling...]" << std::endl;
		}
		m_currentSize = 0;
	}

	if (m_echo) {
		std::cout << lineToLog;
	}

	std::ofstream outFile(m_strFilename.c_str(), std::ios::out | std::ios::app);

	outFile << lineToLog;
	m_currentSize += lineToLog.length();

	return outFile.good();
}

bool Logger::LogAndEcho(const std::string& str, const char* szLogName, const char* szFile, unsigned int nLine)
{
	Logger logger(szLogName);
	logger.SetEcho(true);
	return logger.Log(str, szFile, nLine);
}

bool Logger::LogNoEcho(const std::string& str, const char* szLogName, const char* szFile, unsigned int nLine)
{
	Logger logger(szLogName);
	logger.SetEcho(false);
	return logger.Log(str, szFile, nLine);
}

}
