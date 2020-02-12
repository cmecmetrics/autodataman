///////////////////////////////////////////////////////////////////////////////
///
///	\file    autodataman.cpp
///	\author  Paul Ullrich
///	\version January 10, 2020
///
///	<remarks>
///		Copyright (c) 2020 Paul Ullrich 
///	
///		Distributed under the Boost Software License, Version 1.0.
///		(See accompanying file LICENSE_1_0.txt or copy at
///		http://www.boost.org/LICENSE_1_0.txt)
///	</remarks>

#include <cstdio>
#include <cstdlib>
#include <unistd.h>
#include <pwd.h>
 
#include <curl/curl.h>

#include <string>
#include <vector>
#include <iostream>
#include <iomanip>
#include <fstream>
#include "filesystem_path.h"

#include "Exception.h"
#include "Repository.h"
#include "Namelist.h"

///////////////////////////////////////////////////////////////////////////////

int g_verbosity = 0;

///////////////////////////////////////////////////////////////////////////////

typedef std::map<std::string, int> CommandLineFlagSpec;

typedef std::map<std::string, std::vector<std::string> > CommandLineFlagMap;

typedef std::vector<std::string> CommandLineArgVector;

std::string ParseCommandLine(
	int ibegin,
	int iend,
	char ** argv,
	const CommandLineFlagSpec & spec,
	CommandLineFlagMap & mapFlags,
	CommandLineArgVector & vecArg
) {
	// Flags occur before raw arguments
	bool fReadingFlags = true;

	// Loop through all command line arguments
	for (int c = ibegin; c < iend; c++) {
		_ASSERT(strlen(argv[c]) >= 1);

		// Handle flags
		if (argv[c][0] == '-') {
			if (!fReadingFlags) {
				return std::string("Error: Malformed argument \"") + std::string(argv[c]) + std::string("\"");
			}
			if (strlen(argv[c]) == 1) {
				continue;
			}
			std::string strFlag = argv[c] + 1;
			
			CommandLineFlagSpec::const_iterator iterSpec = spec.find(strFlag);
			if (iterSpec == spec.end()) {
				return std::string("Error: Invalid flag \"") + strFlag + std::string("\"");
			}

			CommandLineFlagMap::iterator iterFlags = mapFlags.find(strFlag);
			if (iterFlags != mapFlags.end()) {
				return std::string("Error: Duplicated flag \"") + strFlag + std::string("\"");
			}

			int nargs = iterSpec->second;
			if (c + nargs >= iend) {
				return std::string("Error: Insufficient arguments for \"")
					+ strFlag + std::string("\"");
			}

			std::vector<std::string> vecFlagArg;
			for (int d = 0; d < nargs; d++) {
				_ASSERT(strlen(argv[c+d]) >= 1);
				if (argv[c+d+1][0] == '-') {
					return std::string("Error: Invalid arguments for \"")
						+ strFlag + std::string("\"");
				}
				vecFlagArg.push_back(argv[c+d+1]);
			}

			mapFlags.insert(CommandLineFlagMap::value_type(strFlag, vecFlagArg));

			c += nargs;

		// Handle raw arguments
		} else {
			if (fReadingFlags) {
				fReadingFlags = false;
			}

			vecArg.push_back(argv[c]);
		}
	}

	return std::string("");
}

///////////////////////////////////////////////////////////////////////////////

static size_t write_data_to_stringstream(
	void * ptr,
	size_t size,
	size_t nmemb,
	void * stream
) {
	char * sz = new char[size*nmemb+1];
	if (sz == 0) {
		return 0;
	}
	sz[size*nmemb] = '\0';
	memcpy(sz, (char*)ptr, size*nmemb);
	std::stringstream & str = *((std::stringstream*)stream);
	str << sz;
	delete[] sz;
	return size*nmemb;
}

///////////////////////////////////////////////////////////////////////////////
/*
static size_t write_data(void *ptr, size_t size, size_t nmemb, void *stream)
{
	size_t written = fwrite(ptr, size, nmemb, (FILE *)stream);
	return written;
}
*/

///////////////////////////////////////////////////////////////////////////////

int curl_download_to_stringstream(
	const std::string & strURL,
	std::stringstream & strFile
) {
	// Verify server exists
	CURL *curl_handle;
 
	curl_global_init(CURL_GLOBAL_ALL);
 
	// init the curl session
	curl_handle = curl_easy_init();
 
	// set URL to get here
	curl_easy_setopt(curl_handle, CURLOPT_URL, strURL.c_str());
 
	// Switch on full protocol/debug output while testing
	curl_easy_setopt(curl_handle, CURLOPT_VERBOSE, 0L);
 
	// disable progress meter, set to 0L to enable it
	curl_easy_setopt(curl_handle, CURLOPT_NOPROGRESS, 1L);
 
	// send all data to this function
	curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, write_data_to_stringstream);
 
	// write the page body to this string stream
	curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, (void*)(&strFile));
 
	// get it!
	curl_easy_perform(curl_handle);
 
	// cleanup curl stuff
	curl_easy_cleanup(curl_handle);
 
	curl_global_cleanup();

	return 0;
}

///////////////////////////////////////////////////////////////////////////////

class AutodatamanRepoMD {

public:
	///	<summary>
	///		Clear the repo.
	///	</summary>
	void clear() {
		m_strVersion = "";
		m_vecDatasetNames.clear();
	}

	///	<summary>
	///		Populate from JSON object.
	///	</summary>
	int from_JSON(
		nlohmann::json & jmeta
	) {
		// Convert to local object
		auto itjrepo = jmeta.find("_REPO");
		if (itjrepo == jmeta.end()) {
			_EXCEPTIONT("Malformed repository metadata file (missing \"_REPO\" key)");
		}
		auto itjtype = itjrepo->find("type");
		if (itjtype == itjrepo->end()) {
			_EXCEPTIONT("Malformed repository metadata file (missing \"_REPO::type\" key)");
		}
		if (itjtype.value() != "autodataman") {
			_EXCEPTIONT("Malformed repository metadata file (invalid \"_REPO::type\" value)");
		}
		auto itjversion = itjrepo->find("version");
		if (itjversion == itjrepo->end()) {
			_EXCEPTIONT("Malformed repository metadata file (missing \"_REPO::version\" key)");
		}
		if (!itjversion.value().is_string()) {
			_EXCEPTIONT("Malformed repository metadata file (\"_REPO::version\" must be type \"string\")");
		}
		m_strVersion = itjversion.value();

		// Datasets
		auto itjdatasets = jmeta.find("_DATASETS");
		if (itjdatasets == jmeta.end()) {
			_EXCEPTIONT("Malformed repository metadata file (missing \"_DATASETS\" key)");
		}
		if (!itjdatasets.value().is_array()) {
			_EXCEPTIONT("Malformed repository metadata file (\"_DATASETS\" must be type \"array\")");
		}

		for (size_t s = 0; s < itjdatasets->size(); s++) {
			nlohmann::json & jdataset = (*itjdatasets)[s];
			if (!jdataset.is_string()) {
				_EXCEPTIONT("Malformed repository metadata file (\"_DATASETS\" must be an array of strings");
			}
			m_vecDatasetNames.push_back(jdataset);
		}

		printf("Repository contains %lu dataset(s).\n", m_vecDatasetNames.size());

		return 0;
	}

	///	<summary>
	///		Populate from server.
	///	</summary>
	int from_server(
		const std::string & strServer
	) {
		// Repo metadata
		std::string strServerMetadata = strServer;
		if (strServer[strServer.length()-1] != '/') {
			strServerMetadata += "/";
		}
		strServerMetadata += "repo.txt";

		// Notify user
		printf("Attempting to download server metadata file \"%s\"\n",
			strServerMetadata.c_str());
 
		// Download the metadata file from the server
		std::stringstream strFile;
		curl_download_to_stringstream(strServerMetadata, strFile);

		// Parse into a JSON object
		printf("Download completed successfully.  Parsing metadata file.\n");

		std::cout << "=============================================" << std::endl;
		std::cout << strFile.str() << std::endl;
		std::cout << "=============================================" << std::endl;

		nlohmann::json jmeta;
		try {
			jmeta = nlohmann::json::parse(strFile.str());
		} catch (nlohmann::json::parse_error& e) {
			_EXCEPTION3("Malformed repository metadata file "
				"%s (%i) at byte position %i",
				e.what(), e.id, e.byte);
		}

		printf("Validating and storing metadata.\n");

		return from_JSON(jmeta);
	}

	///	<summary>
	///		Get the current version number.
	///	</summary>
	const std::string & get_version() const {
		return m_strVersion;
	}

	///	<summary>
	///		Get the vector of dataset names.
	///	</summary>
	const std::vector<std::string> & get_dataset_names() const {
		return m_vecDatasetNames;
	}

protected:
	///	<summary>
	///		Version number.
	///	</summary>
	std::string m_strVersion;

	///	<summary>
	///		List of available datasets.
	///	</summary>
	std::vector<std::string> m_vecDatasetNames;
};

///////////////////////////////////////////////////////////////////////////////

class AutodatamanRepoDatasetMD {

public:
	///	<summary>
	///		Clear the repo.
	///	</summary>
	void clear() {
		m_strShortName = "";
		m_strLongName = "";
		m_strSource = "";
		m_strDefaultVersion = "";
		m_vecDatasetVersions.clear();
	}

	///	<summary>
	///		Populate from JSON object.
	///	</summary>
	int from_JSON(
		nlohmann::json & jmeta
	) {
		// Convert to local object
		auto itjdataset = jmeta.find("_DATASET");
		if (itjdataset == jmeta.end()) {
			_EXCEPTIONT("Malformed repository metadata file (missing \"_DATASET\" key)");
		}

		// Short name
		auto itjshortname = itjdataset->find("short_name");
		if (itjshortname == itjdataset->end()) {
			_EXCEPTIONT("Malformed repository metadata file (missing \"_DATASET::short_name\" key)");
		}
		if (!itjshortname.value().is_string()) {
			_EXCEPTIONT("Malformed repository metadata file (\"_DATASET::short_name\" must be type \"string\")");
		}
		m_strShortName = itjshortname.value();

		// Long name
		auto itjlongname = itjdataset->find("long_name");
		if (itjlongname == itjdataset->end()) {
			_EXCEPTIONT("Malformed repository metadata file (missing \"_DATASET::long_name\" key)");
		}
		if (!itjlongname.value().is_string()) {
			_EXCEPTIONT("Malformed repository metadata file (\"_DATASET::long_name\" must be type \"string\")");
		}
		m_strLongName = itjlongname.value();

		// Source
		auto itjsource = itjdataset->find("source");
		if (itjsource == itjdataset->end()) {
			_EXCEPTIONT("Malformed repository metadata file (missing \"_DATASET::source\" key)");
		}
		if (!itjsource.value().is_string()) {
			_EXCEPTIONT("Malformed repository metadata file (\"_DATASET::source\" must be type \"string\")");
		}
		m_strSource = itjsource.value();

		// Versions
		auto itjversions = jmeta.find("_VERSIONS");
		if (itjversions == jmeta.end()) {
			_EXCEPTIONT("Malformed repository metadata file (missing \"_VERSIONS\" key)");
		}
		if (!itjversions.value().is_array()) {
			_EXCEPTIONT("Malformed repository metadata file (\"_VERSIONS\" must be type \"array\")");
		}

		for (size_t s = 0; s < itjversions->size(); s++) {
			nlohmann::json & jversion = (*itjversions)[s];
			if (!jversion.is_string()) {
				_EXCEPTIONT("Malformed repository metadata file (\"_VERSIONS\" must be an array of strings");
			}
			m_vecDatasetVersions.push_back(jversion);
		}

		printf("Dataset contains %lu version(s).\n", m_vecDatasetVersions.size());

		return 0;
	}

	///	<summary>
	///		Populate from server.
	///	</summary>
	int from_server(
		const std::string & strServer,
		const std::string & strDataset
	) {
		// Repo metadata
		std::string strServerMetadata = strServer;
		if (strServer[strServer.length()-1] != '/') {
			strServerMetadata += "/";
		}
		strServerMetadata += strDataset + std::string("/dataset.txt");
 
		// Download the metadata file from the server
		std::stringstream strFile;
		curl_download_to_stringstream(strServerMetadata, strFile);

		// Parse into a JSON object
		printf("Download completed successfully.  Parsing metadata file.\n");

		std::cout << "=============================================" << std::endl;
		std::cout << strFile.str() << std::endl;
		std::cout << "=============================================" << std::endl;

		nlohmann::json jmeta;
		try {
			jmeta = nlohmann::json::parse(strFile.str());
		} catch (nlohmann::json::parse_error& e) {
			_EXCEPTION3("Malformed repository metadata file "
				"%s (%i) at byte position %i",
				e.what(), e.id, e.byte);
		}

		printf("Validating and storing metadata.\n");

		return from_JSON(jmeta);
	}

	///	<summary>
	///		Get the dataset short name.
	///	</summary>
	const std::string & get_short_name() const {
		return m_strShortName;
	}

	///	<summary>
	///		Get the dataset long name.
	///	</summary>
	const std::string & get_long_name() const {
		return m_strLongName;
	}

	///	<summary>
	///		Get the dataset source.
	///	</summary>
	const std::string & get_source() const {
		return m_strSource;
	}

	///	<summary>
	///		Get the vector of dataset names.
	///	</summary>
	const std::vector<std::string> & get_dataset_names() const {
		return m_vecDatasetNames;
	}

protected:
	///	<summary>
	///		Short name.
	///	</summary>
	std::string m_strShortName;

	///	<summary>
	///		Long name.
	///	</summary>
	std::string m_strLongName;

	///	<summary>
	///		Source.
	///	</summary>
	std::string m_strSource;

	///	<summary>
	///		Default version.
	///	</summary>
	std::string m_strDefaultVersion;

	///	<summary>
	///		List of available datasets.
	///	</summary>
	std::vector<std::string> m_vecDatasetVersions;

};

///////////////////////////////////////////////////////////////////////////////

class AutodatamanNamelist : public Namelist {

public:
	///	<summary>
	///		Constructor.
	///	</summary>
	AutodatamanNamelist() :
		Namelist()
	{
	}

protected:
	///	<summary>
	///		Initialize the autodataman path.
	///	</summary>
	int InitializePath() {
		// Search for $HOME/.autodataman
		char * homedir = getenv("HOME");

		if (homedir != NULL) {
			filesystem::path pathNamelist(homedir);
			if (!pathNamelist.exists()) {
				printf("ERROR environment variable $HOME points to an invalid home directory path\n");
				return 1;
			}
			m_path = pathNamelist/filesystem::path(".autodataman");
			return 0;
		}

		// Search for <pwd>/.autodataman
		uid_t uid = getuid();
		struct passwd *pw = getpwuid(uid);

		if (pw == NULL) {
			printf("ERROR unable to identify .autodataman\n");
			return 1;
		}

		filesystem::path pathNamelist(pw->pw_dir);
		if (!pathNamelist.exists()) {
			printf("ERROR pwd points to an invalid home directory path\n");
			return 1;
		}
		m_path = pathNamelist/filesystem::path(".autodataman");
		return 0;
	}

public:
	///	<summary>
	///		Initialize the autodataman namelist.
	///	</summary>
	int LoadFromUser() {
		int iStatus = InitializePath();
		if (iStatus) return iStatus;

		// If the .autodataman file exists, populate the namelist
		if (m_path.exists()) {
			int iStatus = FromFile(m_path.str());
			if (iStatus) return iStatus;
		}

		return 0;
	}

	///	<summary>
	///		Write the autodataman namelist to its file.
	///	</summary>
	int SaveToUser() {
		return ToFile(m_path.str());
	}

protected:
	///	<summary>
	///		Path to the autodataman namelist.
	///	</summary>
	filesystem::path m_path;
};

///////////////////////////////////////////////////////////////////////////////

int adm_getrepo_string(
	std::string & str
) {
	// Load .autodataman
	AutodatamanNamelist nml;
	int iStatus = nml.LoadFromUser();
	if (iStatus) return iStatus;

	str = nml["default_local_repo"];

	return 0;
}

///////////////////////////////////////////////////////////////////////////////

int adm_getserver_string(
	std::string & str
) {
	// Load .autodataman
	AutodatamanNamelist nml;
	int iStatus = nml.LoadFromUser();
	if (iStatus) return iStatus;

	str = nml["default_server"];

	return 0;
}

///////////////////////////////////////////////////////////////////////////////
 
int adm_initrepo(
	const std::string & strDir
) {
	filesystem::path pathRepo(strDir);

	// Check for repo path
	if (pathRepo.exists()) {
		printf("ERROR creating directory \"%s\": Specified path already exists\n",
			pathRepo.str().c_str());
		return 1;
	}

	// Create the directory
	bool fCreateDir = filesystem::create_directory(pathRepo);
	if (!fCreateDir) {
		printf("ERROR creating directory \"%s\": Failed in call to mkdir\n",
			pathRepo.str().c_str());
		return 1;
	}

	// Create a new JSON file in the directory
	Repository repoNew(strDir);

	repoNew.WriteMetadata();

	// Report success
	printf("New autodataman repo \"%s\" created successfully\n", strDir.c_str());
	return 0;
}

///////////////////////////////////////////////////////////////////////////////
 
int adm_setrepo(
	const std::string & strDir
) {
	filesystem::path pathRepo(strDir);

	// Check for existence of repo path
	if (!pathRepo.exists()) {
		printf("ERROR repo path \"%s\" not found\n",
			pathRepo.str().c_str());
		return 1;
	}

	// Verify directory is a valid repository
	filesystem::path pathRepoMeta = pathRepo/filesystem::path("repo.txt");
	if (!pathRepoMeta.exists()) {
		printf("ERROR \"%s\" is not a valid autodataman repo: Missing \"repo.txt\" file\n",
			pathRepo.str().c_str());
		return 1;
	}

	// Set this repository to be default in .autodataman
	AutodatamanNamelist nml;
	int iStatus = nml.LoadFromUser();
	if (iStatus) return iStatus;

	nml["default_local_repo"] = strDir;

	iStatus = nml.SaveToUser();
	if (iStatus) return iStatus;

	printf("Default autodataman repo set to \"%s\"\n", strDir.c_str());
	return 0;
}

///////////////////////////////////////////////////////////////////////////////
 
int adm_getrepo() {

	// Report the name of the repository in .autodataman
	std::string strRepo;
	int iStatus = adm_getrepo_string(strRepo);
	if (iStatus) return iStatus;

	printf("Default autodataman repo set to \"%s\"\n", strRepo.c_str());

	return 0;
}

///////////////////////////////////////////////////////////////////////////////
 
int adm_setserver(
	const std::string & strServer
) {
	// Download the server listing
	nlohmann::json jmeta;

	AutodatamanRepoMD admrepo;
	int iStatus = admrepo.from_server(strServer);
	if (iStatus != 0) return iStatus;

	printf("Validating server contents.\n");

	printf("WARNING: Validation not implemented...\n");

	// Set this server to be default in .autodataman
	AutodatamanNamelist nml;
	iStatus = nml.LoadFromUser();
	if (iStatus) return iStatus;

	nml["default_server"] = strServer;

	iStatus = nml.SaveToUser();
	if (iStatus) return iStatus;

	printf("SUCCESSFULLY set autodataman server to \"%s\"\n", strServer.c_str());

	return 0;
}

///////////////////////////////////////////////////////////////////////////////
 
int adm_getserver() {

	// Report the name of the repository in .autodataman
	std::string strServer;
	int iStatus = adm_getserver_string(strServer);
	if (iStatus) return iStatus;

	if (strServer == "") {
		printf("No default autodataman server\n");
	} else {
		printf("Default autodataman server set to \"%s\"\n", strServer.c_str());
	}

	return 0;
}

///////////////////////////////////////////////////////////////////////////////
 
int adm_avail(
	const std::string & strServer
) {
	// Load repository descriptor from remote data server
	AutodatamanRepoMD admrepo;
	int iStatus = admrepo.from_server(strServer);

	// List available datasets
	const std::vector<std::string> & vecDatasets = admrepo.get_dataset_names();
	for (int i = 0; i < vecDatasets.size(); i++) {
		printf("  %s\n", vecDatasets[i].c_str());
	}

	return 0;
}

///////////////////////////////////////////////////////////////////////////////
 
int adm_info(
	const std::string & strServer,
	const std::string & strLocalRepo,
	const std::string & strDataset
) {
	// Load repository descriptor from remote data server
	AutodatamanRepoDatasetMD admrepodataset;
	int iStatus = admrepodataset.from_server(strServer, strDataset);

	// List available datasets
	const std::vector<std::string> & vecDatasets = admrepodataset.get_dataset_names();
	for (int i = 0; i < vecDatasets.size(); i++) {
		printf("  %s\n", vecDatasets[i].c_str());
	}

	return 0;
}

///////////////////////////////////////////////////////////////////////////////
 
int adm_list() {
	// Load repository descriptor from local repository

	// List contents of the repository

	return 0;
}

///////////////////////////////////////////////////////////////////////////////
 
int adm_remove() {
	// Verify directory exists in repository

	// Double check with user

	// Load repository descriptor from local repository

	// Remove directory associated with files

	// Write repository descriptor

	return 0;
}

///////////////////////////////////////////////////////////////////////////////
 
int adm_get() {
	// Load source repository descriptor

	// Verify requested dataset exists in the repository

	// Create directory on local repository

	// Download data

	// Load target repository descriptor

	// Add dataset to repository descriptor

	// Write repository descriptor

	return 0;
}

///////////////////////////////////////////////////////////////////////////////

int main(int argc, char **argv) {

	try {

	// Executable
	std::string strExecutable = argv[0];

	// Check command line arguments
	if (argc < 2) {
		printf("Usage:\n");
		printf("%s initrepo <local repo dir>\n", strExecutable.c_str());
		printf("%s setrepo <local repo dir>\n", strExecutable.c_str());
		printf("%s getrepo\n", strExecutable.c_str());
		printf("%s setserver <server>\n", strExecutable.c_str());
		printf("%s getserver\n", strExecutable.c_str());
		printf("%s avail [-s <server>]\n", strExecutable.c_str());
		printf("%s list [-l <local repo dir>]\n", strExecutable.c_str());
		printf("%s remove [-l <local repo dir>] [-s <server>] <dataset id>[/version]\n", strExecutable.c_str());
		printf("%s get [-l <local repo dir>] [-s <server>] <dataset id>[/version]\n", strExecutable.c_str());
		printf("%s put [-l <local repo dir>] [-s <server>] [-c <compression type>] [-v <version>] <dataset dir>\n", strExecutable.c_str());
		return 1;
	}

	// Command
	std::string strCommand = argv[1];

	// Arguments
	std::vector<std::string> vecArg;
	for (int c = 2; c < argc; c++) {
		vecArg.push_back(argv[c]);
	}

	// Initialize a repository directory with no content
	if (strCommand == "initrepo") {
		if (argc != 3) {
			printf("Usage: %s initrepo <local repo dir>\n", strExecutable.c_str());
			return 1;
		}

		std::string strDir(argv[2]);

		return adm_initrepo(strDir);
	}

	// Set the default repository
	if (strCommand == "setrepo") {
		if (argc != 3) {
			printf("Usage: %s setrepo <local repo dir>\n", strExecutable.c_str());
			return 1;
		}

		std::string strDir(argv[2]);

		return adm_setrepo(strDir);
	}

	// Get the default repository
	if (strCommand == "getrepo") {
		if (argc != 2) {
			printf("Usage: %s getrepo\n", strExecutable.c_str());
			return 1;
		}

		return adm_getrepo();
	}

	// Set the default server 
	if (strCommand == "setserver") {
		if (argc != 3) {
			printf("Usage: %s setserver <remote server>\n", strExecutable.c_str());
			return 1;
		}

		std::string strServer(argv[2]);

		return adm_setserver(strServer);
	}

	// Get the default server
	if (strCommand == "getserver") {
		if (argc != 2) {
			printf("Usage: %s getserver\n", strExecutable.c_str());
			return 1;
		}

		return adm_getserver();
	}

	// Set the default server 
	if (strCommand == "setserver") {
		if (argc != 3) {
			printf("Usage: %s setserver <remote server>\n", strExecutable.c_str());
			return 1;
		}

		std::string strServer(argv[2]);

		return adm_setserver(strServer);
	}

	// Check for data available
	if (strCommand == "avail") {
		CommandLineFlagSpec spec;
		spec["s"] = 1;

		CommandLineFlagMap mapFlags;
		CommandLineArgVector vecArgs;

		std::string strParseError =
			ParseCommandLine(2, argc, argv, spec, mapFlags, vecArgs);

		if (vecArgs.size() > 0) {
			strParseError = "Error: Invalid or missing arguments";
		}

		std::string strRemoteServer;
		auto itFlagServer = mapFlags.find("s");
		if (itFlagServer != mapFlags.end()) {
			if (itFlagServer->second.size() != 1) {
				strParseError = "Error: Invalid or missing server name specified with \"-s\"";
			} else {
				strRemoteServer = itFlagServer->second[0];
			}
		} else {
			int iStatus = adm_getserver_string(strRemoteServer);
			if (iStatus != 0) {
				return iStatus;
			}
		}

		if (strParseError != "") {
			printf("%s\nUsage: %s avail [-s <server>]\n",
				strParseError.c_str(), strExecutable.c_str());
			return 1;
		}

		return adm_avail(strRemoteServer);
	}

	// Check for data information
	if (strCommand == "info") {
		CommandLineFlagSpec spec;
		spec["s"] = 1;
		spec["l"] = 1;

		CommandLineFlagMap mapFlags;
		CommandLineArgVector vecArgs;

		std::string strParseError =
			ParseCommandLine(2, argc, argv, spec, mapFlags, vecArgs);

		std::string strDataset;
		if (vecArgs.size() != 1) {
			strParseError = "Error: Invalid or missing arguments";
		} else {
			strDataset = vecArgs[0];
		}

		if (strParseError != "") {
			printf("%s\nUsage: %s avail [-s <server>]\n",
				strParseError.c_str(), strExecutable.c_str());
			return 1;
		}

		std::string strRemoteServer;
		auto itFlagServer = mapFlags.find("s");
		if (itFlagServer != mapFlags.end()) {
			if (itFlagServer->second.size() != 1) {
				strParseError = "Error: Invalid or missing server name specified with \"-s\"";
			} else {
				strRemoteServer = itFlagServer->second[0];
			}
		} else {
			int iStatus = adm_getserver_string(strRemoteServer);
			if (iStatus != 0) {
				return iStatus;
			}
		}

		std::string strLocalRepo;
/*
		auto itFlagServer = mapFlags.find("s");
		if (itFlagServer != mapFlags.end()) {
			if (itFlagServer->second.size() != 1) {
				strParseError = "Error: Invalid or missing server name specified with \"-s\"";
			} else {
				strRemoteServer = itFlagServer->second[0];
			}
		} else {
			int iStatus = adm_getserver_string(strRemoteServer);
			if (iStatus != 0) {
				return iStatus;
			}
		}
*/
		return adm_info(strRemoteServer, strLocalRepo, strDataset);
	}

	// Put data on the server
	if (strCommand == "put") {
		//return adm_put();
	}
 /*
	CURL *curl_handle;
	static const char *pagefilename = "page.out";
	FILE *pagefile;
 
	curl_global_init(CURL_GLOBAL_ALL);
 
	// init the curl session
	curl_handle = curl_easy_init();
 
	// set URL to get here
	curl_easy_setopt(curl_handle, CURLOPT_URL, argv[1]);
 
	// Switch on full protocol/debug output while testing
	curl_easy_setopt(curl_handle, CURLOPT_VERBOSE, 1L);
 
	// disable progress meter, set to 0L to enable it
	curl_easy_setopt(curl_handle, CURLOPT_NOPROGRESS, 1L);
 
	// send all data to this function
	curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, write_data);
 
	// open the file
	pagefile = fopen(pagefilename, "wb");
	if(pagefile) {
 
		// write the page body to this file handle
		curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, pagefile);
 
		// get it!
		curl_easy_perform(curl_handle);
 
		// close the header file
		fclose(pagefile);
	}
 
	// cleanup curl stuff
	curl_easy_cleanup(curl_handle);
 
	curl_global_cleanup();
 */

	} catch(Exception & e) {
		std::cout << e.ToString() << std::endl;
		return 1;

	} catch(...) {
		return 1;
	}

	return 0;
}
 
///////////////////////////////////////////////////////////////////////////////

