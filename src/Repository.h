///////////////////////////////////////////////////////////////////////////////
///
///	\file    Repository.h
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

#ifndef _REPOSITORY_H_
#define _REPOSITORY_H_

#include <string>
#include <vector>
#include <iostream>
#include <iomanip>
#include <fstream>

#include "filesystem_path.h"
#include "contrib/json.hpp"

///////////////////////////////////////////////////////////////////////////////

class RepositroyDatasetInstance {


public:
	///	<summary>
	///		Version id of the dataset instance.
	///	</summary>
	std::string m_strVersion;
};

///////////////////////////////////////////////////////////////////////////////

class RepositoryDataset {

public:
	///	<summary>
	///		Identifier for this dataset.
	///	</summary>
	std::string m_strId;

	///	<summary>
	///		A map between version ids and relative paths.
	///	</summary>
	std::vector<RepositroyDatasetInstance> m_vecVersionInstance;
};

///////////////////////////////////////////////////////////////////////////////

///	<summary>
///		A class describing a repository of observational data.
///	</summary>
class Repository {

public:
	///	<summary>
	///		Constructor with path.
	///	</summary>
	Repository(
		const filesystem::path & path 
	) : m_path(path)
	{ }

public:
	///	<summary>
	///		Destructor.
	///	</summary>
	~Repository() {
		for (auto iter = m_vecDatasets.begin(); iter != m_vecDatasets.end(); iter++) {
			delete *iter;
		}
	}

public:
	///	<summary>
	///		Initialize the repository from a JSON file.
	///	</summary>
	void FromJSON(
		const std::string & strFile
	);

	///	<summary>
	///		Write the repository to a JSON file.
	///	</summary>
	int WriteMetadata(
		bool fPrettyPrint = false
	) {
		std::string strJSONOutputFilename =
			(m_path/filesystem::path("repo.json")).str();

		std::ofstream ofJSON(strJSONOutputFilename.c_str());
		if (!ofJSON.is_open()) {
			printf("ERROR opening file \"%s\" for writing",
				strJSONOutputFilename.c_str());
			return 1;
		}

		nlohmann::json j;

		j["type"] = "autodataman";
		j["version"] = "0.1";

		// Output to the file stream
		if (fPrettyPrint) {
			ofJSON << std::setw(4) << j;
		} else {
			ofJSON << j;
		}

		return 0;
	}

protected:
	///	<summary>
	///		Path to the repo, if local.
	///	</summary>
	filesystem::path m_path;

	///	<summary>
	///		A vector of repository datasets.
	///	</summary>
	std::vector<RepositoryDataset *> m_vecDatasets;
};

///////////////////////////////////////////////////////////////////////////////

#endif // _REPOSITORY_H_

