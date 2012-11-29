
//           Copyright 2007-2008 Juha Reunanen
//                     2008-2011 Numcore Ltd
//
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#ifdef WIN32
#pragma warning (disable: 4786)
#endif // WIN32

#include "IniFile.h"

#include <sstream>
#include <iomanip>

#include <assert.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>

namespace {

const std::string whiteSpaces( " \f\n\r\t\v" );

void TrimRight( std::string& str, const std::string& trimChars = whiteSpaces )
{
   std::string::size_type pos = str.find_last_not_of( trimChars );
   str.erase( pos + 1 );
}

void TrimLeft( std::string& str, const std::string& trimChars = whiteSpaces )
{
   std::string::size_type pos = str.find_first_not_of( trimChars );
   str.erase( 0, pos );
}

void Trim( std::string& str, const std::string& trimChars = whiteSpaces )
{
   TrimRight( str, trimChars );
   TrimLeft( str, trimChars );
}

}

namespace numcfc {

IniFile::IniFile(const char* szFilename /* = NULL */)
{
	m_tFileModified = 0;
	m_dirty = false;

    if (szFilename) {
        Load(szFilename);
    }
}

IniFile::~IniFile(void)
{
}

void IniFile::SetFilename(const char* szFilename)
{
	m_tFileModified = 0;
	m_defaultFilename = szFilename;

#ifdef WIN32
	struct _stat s;
	bool bStatOk = (_stat(m_defaultFilename.c_str(), &s) == 0);
#else
	struct stat s;
	bool bStatOk = (stat(m_defaultFilename.c_str(), &s) == 0);
#endif
	if (!bStatOk) {
		std::ofstream outFile(szFilename);
	}
}

const std::string& IniFile::GetFilename() const
{
	return m_defaultFilename;
}

bool IniFile::Refresh()
{
	bool retVal = false;
#ifdef WIN32
	struct _stat s;
	bool bStatOk = (_stat(m_defaultFilename.c_str(), &s) == 0);
#else
	struct stat s;
	bool bStatOk = (stat(m_defaultFilename.c_str(), &s) == 0);
#endif
	if (s.st_mtime != m_tFileModified) {
		if (Load(m_defaultFilename.c_str())) {
			m_tFileModified = s.st_mtime;
			retVal = true;
		}
	}
	return retVal;
}

bool IniFile::Load(const char* szFilename)
{	
	std::string filename;
    if (szFilename) {
        filename = szFilename;
        m_defaultFilename = szFilename;
    }
    else {
        filename = m_defaultFilename;
    }
    
	std::ifstream inFile(filename.c_str());
	if (!inFile.good()) {
		return false;
	}

	inFile >> *this;

	return inFile.eof();
}

std::istream& operator >>(std::istream& istr, IniFile& iniFile) 
{
	if (!istr) {
		return istr;
	}

	// Current line
	std::string s;
	// Current section name
	std::string currentSection;

	iniFile.m_contents.clear();

	std::string comments = "";

	while (std::getline(istr, s))
	{
		Trim(s);
		if (!s.empty())
		{
			IniFile::Record r;

			// Is this a commented line?
			if (s[0]=='#' || s[0]==';')
			{
				comments += s + '\n';
			}

			// Does this line start a new section?			   
			else if (s.find('[') != std::string::npos && (s.find('=') == std::string::npos || s.find('[') < s.find('=')))
			{
				// erase the brackets...
				TrimLeft(s, "[" + whiteSpaces);
				TrimRight(s, "]" + whiteSpaces);

				r.m_sectionName = s;											
				r.m_keyName = "";
				r.m_value = "";
				r.m_comment = comments;
				comments = "";
				currentSection = s;

				iniFile.m_contents.push_back(r);
			}
			
			else {
				// Key/value?
				size_t eqPos = s.find('=');
				if (eqPos != std::string::npos)
				{
					r.m_sectionName = currentSection;
					r.m_keyName = s.substr(0, eqPos); // The key is everything before the equals sign
					r.m_value = s.substr(eqPos + 1);  // The value is everything after the equals sign
					Trim(r.m_keyName);
					Trim(r.m_value);
					r.m_comment = comments;
					comments = "";

					iniFile.m_contents.push_back(r);
				}
			}
		}
	}
	
	return istr;
}

bool IniFile::Save(const char* szFilename) const
{
    std::string filename;
    if (szFilename) {
        filename = szFilename;
    }
    else {
        filename = m_defaultFilename;
    }

	std::ofstream outFile(filename.c_str());
	if (!outFile.good()) {
		return false;
	}

	outFile << *this;

	bool good = outFile.good();

	if (good) {
		m_dirty = false;
		m_tFileModified = time(NULL);
	}

	return good;
}

std::ostream& operator <<(std::ostream& ostr, const IniFile& iniFile)
{
	std::vector<IniFile::Record>::const_iterator i = iniFile.m_contents.begin(), iEnd = iniFile.m_contents.end();
	for (; i != iEnd; ++i) {
		const IniFile::Record& r = *i;
		if (r.IsSection() || !r.m_comment.empty()) {
			if (i != iniFile.m_contents.begin()) {
				ostr << std::endl;
			}
		}
		if (!r.m_comment.empty()) {
			ostr << r.m_comment;
		}
		if (r.IsSection()) {
			ostr << "[" << r.m_sectionName << "]";
		}
		else {
			ostr << r.m_keyName << " = " << r.m_value;
		}
		ostr << std::endl;
	}

	return ostr;
}

bool IniFile::IsDirty() const
{
	return m_dirty;
}

std::string IniFile::GetValue(const std::string& sectionName, const std::string& keyName) const
{
	std::vector<Record>::const_iterator i = m_contents.begin(), iEnd = m_contents.end();
	for (; i != iEnd; ++i) {
		if (i->m_keyName == keyName && i->m_sectionName == sectionName) {
			return i->m_value;
		}
	}
	return "";
}

std::string IniFile::GetSetValue(const std::string& sectionName, const std::string& keyName, const std::string& defaultValue, const std::string& defaultComment)
{
	std::vector<Record>::const_iterator i = m_contents.begin(), iEnd = m_contents.end();
	for (; i != iEnd; ++i) {		
		if (i->m_keyName == keyName && i->m_sectionName == sectionName) {
			return i->m_value;
		}
	}
	Record& r = EditRecord(sectionName, keyName);

	std::string comment = PrepareComment(defaultComment);

	bool changes = (r.m_value != defaultValue || r.m_comment != comment);
	if (changes) {
		m_dirty = true;
		r.m_value = defaultValue;
		r.m_comment = comment;
	}
	return defaultValue;
}

double IniFile::GetSetValue(const std::string& sectionName, const std::string& keyName, const double& defaultValue, const std::string& defaultComment)
{
	std::ostringstream ostr;
	ostr << std::setprecision(20) << defaultValue;
	std::string s = GetSetValue(sectionName, keyName, ostr.str(), defaultComment);
	double d = atof(s.c_str());
	return d;
}

void IniFile::SetValue(const std::string& sectionName, const std::string& keyName, const std::string& value)
{
	Record& r = EditRecord(sectionName, keyName);
	if (r.m_value != value) {
		r.m_value = value;
		m_dirty = true;
	}
}

void IniFile::SetValue(const std::string& sectionName, const std::string& keyName, double value)
{
	std::ostringstream ostr;
	ostr << std::setprecision(30) << value;
	SetValue(sectionName, keyName, ostr.str());
}

IniFile::Record& IniFile::EditRecord(const std::string& sectionName, const std::string& keyName)
{
	std::vector<Record>::iterator i;

	for (i = m_contents.begin(); i != m_contents.end(); i++) {
		if (i->m_sectionName == sectionName) {
			break;
		}
	}
	
	if (i == m_contents.end())											
	{ 
		// section doesn't exist: add
		m_dirty = true;

		Record r;

		r.m_sectionName = sectionName;
		m_contents.push_back(r);

		r.m_keyName = keyName;
		m_contents.push_back(r);

		return m_contents.back();
	}
	else {
		// search for the key
		for (; i != m_contents.end() && i->m_sectionName == sectionName; i++) {
			if (i->m_keyName == keyName) {
				return *i;
			}
		}

		// key doesn't exist
		m_dirty = true;

		Record r;
		r.m_sectionName = sectionName;
		r.m_keyName = keyName;

		std::vector<Record>::iterator iInserted = m_contents.insert(i, r);
		return *iInserted;
	}
}

bool IniFile::DeleteRecord(const std::string& sectionName, const std::string& keyName)
{
	std::vector<Record>::iterator i;

	for (i = m_contents.begin(); i != m_contents.end(); i++) {
		if (i->m_sectionName == sectionName) {
			break;
		}
	}
	
	if (i != m_contents.end())											
	{ 
		// section found
		// search for the key
		for (; i != m_contents.end() && i->m_sectionName == sectionName; i++) {
			if (i->m_keyName == keyName) {
				// key found
				m_dirty = true;
				m_contents.erase(i);
				return true;
			}
		}
	}

	return false;
}

bool IniFile::DeleteSection(const std::string& sectionName)
{
	std::vector<Record>::iterator i;

	for (i = m_contents.begin(); i != m_contents.end(); i++) {
		if (i->m_sectionName == sectionName) {
			break;
		}
	}

	if (i == m_contents.end()) {
		// section not found
		return false;
	}

	std::vector<std::vector<Record>::iterator> iteratorsToErase;

	do { 
		iteratorsToErase.push_back(i++);
	} while (i != m_contents.end() && i->m_sectionName == sectionName);

	assert(!iteratorsToErase.empty());

	// erase in reverse order, in order to keep the vector iterators valid
	for (std::vector<std::vector<Record>::iterator>::const_reverse_iterator it = iteratorsToErase.rbegin(), itEnd = iteratorsToErase.rend(); it != itEnd; ++it) {
		std::vector<Record>::iterator iToErase = *it;
		m_contents.erase(iToErase);
	}

	m_dirty = true;
	return true;
}

bool IniFile::SetComment(const std::string& sectionName, const std::string& keyName, const std::string& comment)
{
	std::vector<Record>::iterator i;

	for (i = m_contents.begin(); i != m_contents.end(); i++) {
		if (i->m_sectionName == sectionName) {
			break;
		}
	}
	
	if (i != m_contents.end())											
	{ 
		// section found
		// search for the key
		for (; i != m_contents.end() && i->m_sectionName == sectionName; i++) {
			if (i->m_keyName == keyName) {
				// key found
				std::string preparedComment = PrepareComment(comment);
				if (i->m_comment != preparedComment) {
					m_dirty = true;
					i->m_comment = preparedComment;
				}
				return true;
			}
		}
	}

	return false;
}

/*
std::string IniFile::GetComment(const std::string& sectionName, const std::string& keyName)
{
	std::vector<Record>::const_iterator i = m_contents.begin(), iEnd = m_contents.end();
	for (; i != iEnd; ++i) {		
		if (i->m_keyName == keyName && i->m_sectionName == sectionName) {
			std::string comment = i->m_comment;
			if (comment.length() >= 2) {
				std::string commentStart = comment.substr(0, 2);
				if (commentStart == "# " || commentStart == "; ") {
					// strip the comment prefix
					comment = comment.substr(2);
				}
			}
			return comment;
		}
	}
	return "";
}
*/

std::vector<std::string> IniFile::GetSections() const
{
	std::vector<std::string> sections;
	std::string currentSection;
	std::vector<Record>::const_iterator i = m_contents.begin(), iEnd = m_contents.end();
	for (; i != iEnd; ++i) {		
		if (i->m_sectionName != currentSection) {
			assert(std::find(sections.begin(), sections.end(), i->m_sectionName) == sections.end());
			sections.push_back(i->m_sectionName);
			currentSection = i->m_sectionName;
		}
	}
	return sections;
}

std::vector<std::string> IniFile::GetKeys(const std::string& sectionName) const
{
	std::vector<std::string> keys;
	std::vector<Record>::const_iterator i = m_contents.begin(), iEnd = m_contents.end();
	for (; i != iEnd; ++i) {		
		if (i->m_sectionName == sectionName && !i->IsSection()) {
			keys.push_back(i->m_keyName);
		}
	}
	return keys;
}

std::string IniFile::PrepareComment(const std::string& inputComment)
{
	std::string comment = inputComment;
	if (!comment.empty()) {
		std::string::size_type index = 0;
		while (index != std::string::npos) {
			if (index > 0) {
				++index;
			}
			char c = comment[index];
			if (c != '#' && c != ';') {
				comment = comment.substr(0, index) + "# " + comment.substr(index);
			}
			index = comment.find_first_of("\n", index);
		}

		char cLast = comment[comment.length() - 1];
		if (cLast != '\n') {
			comment += "\n";
		}
	}
	return comment;
}

bool IniFile::FromString(const std::string& str)
{
	std::istringstream iss(str);
	iss >> *this;
	return !!iss;
}

std::string IniFile::ToString() const
{
	std::ostringstream oss;
	oss << *this;
	return oss.str();
}

}
