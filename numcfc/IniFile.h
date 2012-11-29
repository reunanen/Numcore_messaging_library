
//           Copyright 2007-2008 Juha Reunanen
//                     2008-2011 Numcore Ltd
//
// Distributed under the Boost Software License, Version 1.0.
//    (See accompanying file LICENSE_1_0.txt or copy at
//          http://www.boost.org/LICENSE_1_0.txt)

#ifndef NUMCFC_INIFILE_H
#define NUMCFC_INIFILE_H

#include <string>
#include <vector>
#include <iostream>
#include <fstream>
#include <algorithm>
#include <functional>

namespace numcfc {

//! A helper class to get and set parameters stored in ini files.
/*! Ini files are easy enough for most users to set the less-frequently modified or advanced parameters,
	for which it is not worth the effort to write the user interface controls.
*/
class IniFile
{
public:

	//! The constructor.
	/*! \param szFilename The default filename for Refresh(), Load() and Save() operations.
	*/
	IniFile(const char* szFilename = NULL);

	virtual ~IniFile(void);

	//! Set the default filename for Refresh(), Load() and Save() operations.
	void SetFilename(const char* szFilename);

	//! Get the current default filename for Refresh(), Load() and Save() operations.
	const std::string& GetFilename() const;

	//! Load the contents from a file.
	/*! \param szFilename If NULL; uses the filename previously set by the constructor, SetFilename(), or Load().
	*/
	bool Load(const char* szFilename);

	//! Save the (possibly dirty) memory contents to file.
	/*! \param szFilename If NULL, uses the filename set by the constructor, SetFilename(), or Load().
	*/
    bool Save(const char* szFilename = NULL) const;

	//! If the object IsDirty(), one is normally supposed to Save() it.
	/*! The in-memory contents have been modified using SetValue() or GetSetValue().
	*/
	bool IsDirty() const;

	//! Update the on-disk changes to memory.
	/*! \return true, if the contents have changed since the previous call to Refresh().
	*/
	bool Refresh();

	//! Get a value from memory.
	/*!	\param sectionName The ini file section name, which behaves like a heading.
		\param keyName The ini file key name, i.e., the name of a single item or parameter.
		\return The current value, or empty if not defined.
	*/
	std::string GetValue(const std::string& sectionName, const std::string& keyName) const;

	//! Set a string value in memory.
	/*!	\param sectionName The ini file section name, which behaves like a heading.
		\param keyName The ini file key name, i.e., the name of a single item or parameter.
		\param value The value of the parameter specified by sectionName and keyName.
	*/
	void SetValue(const std::string& sectionName, const std::string& keyName, const std::string& value);

	//! Set a numeric value in memory.
	/*!	\param sectionName The ini file section name, which behaves like a heading.
		\param keyName The ini file key name, i.e., the name of a single item or parameter.
		\param value The value of the parameter specified by sectionName and keyName.
	*/
	void SetValue(const std::string& sectionName, const std::string& keyName, double value);

	//! Same as GetValue() if the section/key pair exists; if not, set the default value.
	/*!	\param sectionName The ini file section name, which behaves like a heading.
		\param keyName The ini file key name, i.e., the name of a single item or parameter.
		\param defaultValue The value that will be set if the section/key pair does not exist.
		\param defaultComment The default comment to be added in the ini file, if defaultValue is set.
		\return The current value, or the default value.
	*/
	std::string GetSetValue(const std::string& sectionName, const std::string& keyName, const std::string& defaultValue, const std::string& defaultComment = "");

	double GetSetValue(const std::string& sectionName, const std::string& keyName, const double& defaultValue, const std::string& defaultComment = "");

	bool DeleteRecord(const std::string& sectionName, const std::string& keyName);

	bool DeleteSection(const std::string& sectionName);

	/*!
		\return true, if key exists
	*/
	bool SetComment(const std::string& sectionName, const std::string& keyName, const std::string& comment);

	//! COMMENTED OUT AS UNNECESSARY AND NOT SUPPORTED.
	/*!
		\return empty if the section, the key, or the comment does not exist
	*/
	//std::string GetComment(const std::string& sectionName, const std::string& keyName);

	/*!
		\return all section names
	*/
	std::vector<std::string> GetSections() const;

	/*!
		\return the key names of the desired section
	*/
	std::vector<std::string> GetKeys(const std::string& sectionName) const;

	//! Load from a string.
	bool FromString(const std::string& str);

	//! Save to a string. Does NOT clear the dirty bit.
	std::string ToString() const;

	//! Load the contents from an input stream.
	friend std::istream& operator >>(std::istream& istr, IniFile& iniFile);

	//! Save the memory contents to an output stream. Does NOT clear the dirty bit.
	friend std::ostream& operator <<(std::ostream& ostr, const IniFile& iniFile);

private:
	class Record
	{
	public:
		Record() { };

		bool IsSection() const { return m_keyName.empty(); };

		std::string m_sectionName;
		std::string m_keyName;
		std::string m_value;

		std::string m_comment;
	};

	// add if doesn't exist
	Record& EditRecord(const std::string& sectionName, const std::string& keyName);

	static std::string PrepareComment(const std::string& inputComment);

    std::string m_defaultFilename;
    std::vector<Record> m_contents;

	mutable time_t m_tFileModified;
	mutable bool m_dirty; // needed to make Save() a const method
};

}

#endif // NUMCFC_INIFILE_H