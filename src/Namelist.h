///////////////////////////////////////////////////////////////////////////////
///
///	\file    Namelist.h
///	\author  Paul Ullrich
///	\version January 13, 2020
///
///	<remarks>
///		Copyright (c) 2020 Paul Ullrich 
///	
///		Distributed under the Boost Software License, Version 1.0.
///		(See accompanying file LICENSE_1_0.txt or copy at
///		http://www.boost.org/LICENSE_1_0.txt)
///	</remarks>

#ifndef _NAMELIST_H_
#define _NAMELIST_H_

#include <string>
#include <vector>
#include <iostream>
#include <iomanip>
#include <fstream>
#include <cctype>

///////////////////////////////////////////////////////////////////////////////

class Namelist {

public:
	///	<summary>
	///		Key value pair.
	///	</summary>
	typedef std::pair<std::string, std::string> KeyValuePair;

private:
	///	<summary>
	///		Remove whitespace from the string.
	///	</summary>
	static void RemoveWhitespaceFromString(
		std::string & str
	) {
		int iBegin = 0;
		int iEnd = str.length();

		for (int i = 0; i < str.length(); i++) {
			if ((str[i] == ' ') || (str[i] == '\t')) {
				iBegin++;
			} else {
				break;
			}
		}
		for (int i = iEnd-1; i >= 0; i--) {
			if ((str[i] == ' ') || (str[i] == '\t')) {
				iEnd--;
			} else {
				break;
			}
		}
		if (iEnd <= iBegin) {
			str = "";
		} else {
			str = str.substr(iBegin, iEnd-iBegin);
		}
	}

public:
	///	<summary>
	///		Read a namelist from a file.
	///	</summary>
	int FromFile(
		const std::string & strFile
	) {
		// Check that the Namelist is empty
		if (m_vecKeyValuePairs.size() != 0) {
			printf("ERROR Namelist not empty");
			return 1;
		}

		// Open the set
		std::ifstream ifNamelist(strFile.c_str());
		if (!ifNamelist.is_open()) {
			printf("ERROR opening file \"%s\" for reading",
				strFile.c_str());
			return 1;
		}

		// Parse the namelist file line-by-line
		int iLine = 1;
		std::string strLine;
		while (std::getline(ifNamelist, strLine)) {

			// Extract the key and value from the line
			int iEqualIx = (-1);
			for (int i = 0; i < strLine.length(); i++) {
				if (strLine[i] == '=') {
					if (iEqualIx != (-1)) {
						printf("ERROR namelist file \"%s\" contains multiple equal signs\n",
							strFile.c_str());
						return 1;
					}
					iEqualIx = i;
				}
				if (strLine[i] == '#') {
					strLine = strLine.substr(0, i);
					break;
				}
			}

			std::string strKey;
			std::string strValue;

			if (iEqualIx == (-1)) {
				strKey = strLine;
				strValue = "";
			} else {
				strKey = strLine.substr(0, iEqualIx);
				strValue = strLine.substr(iEqualIx+1);
			}

			// Remove whitespace on key and value
			RemoveWhitespaceFromString(strKey);
			RemoveWhitespaceFromString(strValue);

			if ((iEqualIx == (-1)) && (strKey != "")) {
				printf("ERROR malformed namelist file \"%s\" on line %i\n",
					strFile.c_str(), iLine);
				return 1;
			}

			if ((iEqualIx != (-1)) && (strKey == "")) {
				printf("ERROR malformed namelist file \"%s\" on line %i\n",
					strFile.c_str(), iLine);
				return 1;
			}

			if (strKey == "") {
				continue;
			}

			// Check for invalid characters in key
			for (int i = 0; i < strKey.length(); i++) {
				if ((i == 0) && !(isalpha(strKey[i]) || (strKey[i] == '_'))) {
					printf("ERROR malformed key in namelist file \"%s\" on line %i\n",
						strFile.c_str(), iLine);
					return 1;
				}
				if (!isalnum(strKey[i]) && (strKey[i] != '_')) {
					printf("ERROR malformed key in namelist file \"%s\" on line %i\n",
						strFile.c_str(), iLine);
					return 1;
				}
			}

			// Insert key/value pair into namelist
			m_vecKeyValuePairs.push_back(
				KeyValuePair(strKey, strValue));

			iLine++;
		}

		return 0;
	}

	///	<summary>
	///		Write the namelist to a file.
	///	</summary>
	int ToFile(
		const std::string & strFile
	) const {

		// Open the set
		std::ofstream ofNamelist(strFile.c_str());
		if (!ofNamelist.is_open()) {
			printf("ERROR opening file \"%s\" for writing",
				strFile.c_str());
			return 1;
		}

		for (auto iter = m_vecKeyValuePairs.begin(); iter != m_vecKeyValuePairs.end(); iter++) {
			ofNamelist << iter->first << "=" << iter->second << std::endl;
		}

		return 0;
	}

	///	<summary>
	///		Square bracket operator.
	///	</summary>
	std::string & operator[](const std::string & str) {
		for (auto iter = m_vecKeyValuePairs.begin(); iter != m_vecKeyValuePairs.end(); iter++) {
			if (iter->first == str) {
				return iter->second;
			}
		}
		m_vecKeyValuePairs.push_back(
			KeyValuePair(str, std::string("")));

		return (m_vecKeyValuePairs[m_vecKeyValuePairs.size()-1].second);
	}

public:
	///	<summary>
	///		Version id of the dataset instance.
	///	</summary>
	std::vector<KeyValuePair> m_vecKeyValuePairs;
};

///////////////////////////////////////////////////////////////////////////////

#endif // _NAMELIST_H_

