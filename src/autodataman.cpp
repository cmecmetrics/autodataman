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
 
#include "admcurl.h"
#include "admmetadata.h"

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
 
int adm_list(
	const std::string & strRepo
) {
	// Local repository path
	printf("Displaying information for repo \"%s\"\n", strRepo.c_str());

	// Load repository descriptor from remote data server
	AutodatamanRepoMD admrepo;
	int iStatus = admrepo.from_local_repo(strRepo);

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
	{
		// Load repository descriptor from remote data server
		AutodatamanRepoDatasetMD admrepodataset;
		int iStatus = admrepodataset.from_server(strServer, strDataset);

		// Output information
		printf("== SERVER COPY ==============================\n");
		printf("Long name:  %s\n", admrepodataset.get_long_name().c_str());
		printf("Short name: %s\n", admrepodataset.get_short_name().c_str());
		printf("Source:     %s\n", admrepodataset.get_source().c_str());
		printf("Versions available:\n");

		// List available datasets
		const std::vector<std::string> & vecVersions = admrepodataset.get_dataset_versions();
		for (int i = 0; i < vecVersions.size(); i++) {
			if (vecVersions[i] == admrepodataset.get_default_version()) {
				printf("  %s [default]\n", vecVersions[i].c_str());
			} else {
				printf("  %s\n", vecVersions[i].c_str());
			}
		}
		printf("=============================================\n");
	}

	{
		// Load repository descriptor from local data server
		AutodatamanRepoDatasetMD admrepodataset;
		int iStatus = admrepodataset.from_local_repo(strLocalRepo, strDataset);

		// Output information
		printf("== LOCAL COPY ===============================\n");
		printf("Long name:  %s\n", admrepodataset.get_long_name().c_str());
		printf("Short name: %s\n", admrepodataset.get_short_name().c_str());
		printf("Source:     %s\n", admrepodataset.get_source().c_str());
		printf("Versions available:\n");

		// List available datasets
		const std::vector<std::string> & vecVersions = admrepodataset.get_dataset_versions();
		for (int i = 0; i < vecVersions.size(); i++) {
			if (vecVersions[i] == admrepodataset.get_default_version()) {
				printf("  %s [default]\n", vecVersions[i].c_str());
			} else {
				printf("  %s\n", vecVersions[i].c_str());
			}
		}
		printf("=============================================\n");
	}

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
 
int adm_get(
	const std::string & strServer,
	const std::string & strLocalRepo,
	const std::string & strDataset
) {
	// Break up dataset into name and version
	std::string strDatasetName;
	std::string strDatasetVersion;
	if (strDataset.length() == 0) {
		_EXCEPTIONT("Missing dataset name\n");
	}
	for (int i = 0; i < strDataset.length(); i++) {
		if (strDataset[i] == '/') {
			if ((strDatasetName != "") || (strDatasetVersion != "")) {
				_EXCEPTIONT("Multiple forward-slash characters in dataset specifier\n");
			}
			if (i == 0) {
				_EXCEPTIONT("Missing dataset name\n");
			}
			if (i == strDataset.length()-1) {
				_EXCEPTIONT("Missing dataset version\n");
			}
			strDatasetName = strDataset.substr(0, i-1);
			strDatasetVersion = strDataset.substr(i+1);
		}
	}
	if (strDatasetName == "") {
		strDatasetName = strDataset;
	}

	// Load repository descriptor from remote data server
	AutodatamanRepoMD admserver;
	int iStatus = admserver.from_server(strServer);
	_ASSERT(iStatus == 0);

	// Verify requested dataset exists in the repository
	int iServerDataset = admserver.find_dataset(strDatasetName);
	if (iServerDataset == (-1)) {
		_EXCEPTION1("Dataset \"%s\" not found on remote server\n",
			strDataset.c_str());
	}

	// Load dataset descriptor from remote data server
	AutodatamanRepoDatasetMD admserverdataset;
	iStatus = admserverdataset.from_server(strServer, strDataset);
	_ASSERT(iStatus == 0);

	// Use default version number, if not specified
	if (strDatasetVersion == "") {
		strDatasetVersion = admserverdataset.get_default_version();
		if (strDatasetVersion == "") {
			_EXCEPTION1("No default version of dataset \"%s\" available on server",
				strDatasetName.c_str());
		}
		printf("Default dataset version \"%s\"\n", strDatasetVersion.c_str());
	}

	// Verify requested version exists in the repository
	int iServerVersion = admserverdataset.find_version(strDatasetVersion);
	if (iServerVersion == (-1)) {
		_EXCEPTION1("Version \"%s\" not found on remote server\n",
			strDatasetVersion.c_str());
	}

	// Download data descriptor from remote data server
	AutodatamanRepoDataMD admserverdata;
	iStatus = admserverdata.from_server(strServer, strDataset, strDatasetVersion);
	_ASSERT(iStatus == 0);

	// Load repository descriptor from local data server
	AutodatamanRepoMD admlocalrepo;
	iStatus = admlocalrepo.from_local_repo(strLocalRepo);
	_ASSERT(iStatus == 0);

	// Check if dataset exists in the local repository
	AutodatamanRepoDatasetMD admlocaldataset;

	int iLocalDataset = admlocalrepo.find_dataset(strDatasetName);
	if (iLocalDataset == (-1)) {
		// Create dataset directory
		filesystem::path pathDataset =
			filesystem::path(strLocalRepo)/filesystem::path(strDatasetName);
		bool fSuccess = filesystem::create_directory(pathDataset);
		if (!fSuccess) {
			_EXCEPTION1("Unable to create directory \"%s\"",
				pathDataset.str().c_str());
		}

		// Copy metadata
		admlocaldataset = admserverdataset;

		// Write metadata

	} else {
		// Load the metadata file
		admlocaldataset.from_local_repo(strLocalRepo, strDatasetName);
	}

	// Check if version exists in the local repository
	AutodatamanRepoDataMD admlocaldata;

	int iLocalVersion = admlocaldataset.find_version(strDatasetVersion);
	if (iLocalVersion == (-1)) {
	} else {
		// Load the metadata file
		admlocaldata.from_local_repo(strLocalRepo, strDatasetName, strDatasetVersion);

		// Check for equivalence
		if (admserverdata == admlocaldata) {
			printf("Specified dataset \"%s\" already exists on local repo.  Overwrite? [y/N] ",
				strDataset.c_str());
		} else {
			printf("== SERVER COPY ==============================\n");
			admserverdata.summary();
			printf("== LOCAL COPY ===============================\n");
			admlocaldata.summary();
			printf("=============================================\n");
			printf("WARNING: Specified dataset \"%s\" exists on local repo, "
				"but metadata descriptor does not match.  This could mean "
				"that one or both datasets are corrupt.  Overwrite? [Y/n] \n", strDataset.c_str());
		}
	}

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
		itFlagServer = mapFlags.find("l");
		if (itFlagServer != mapFlags.end()) {
			if (itFlagServer->second.size() != 1) {
				strParseError = "Error: Invalid or missing server name specified with \"-l\"";
			} else {
				strLocalRepo = itFlagServer->second[0];
			}
		} else {
			int iStatus = adm_getrepo_string(strLocalRepo);
			if (iStatus != 0) {
				return iStatus;
			}
		}

		return adm_info(strRemoteServer, strLocalRepo, strDataset);
	}

	// Check for data on the local repository
	if (strCommand == "list") {
		CommandLineFlagSpec spec;
		spec["l"] = 1;

		CommandLineFlagMap mapFlags;
		CommandLineArgVector vecArgs;

		std::string strParseError =
			ParseCommandLine(2, argc, argv, spec, mapFlags, vecArgs);

		if (vecArgs.size() > 0) {
			strParseError = "Error: Invalid or missing arguments";
		}

		std::string strLocalRepo;
		auto itFlagServer = mapFlags.find("l");
		if (itFlagServer != mapFlags.end()) {
			if (itFlagServer->second.size() != 1) {
				strParseError = "Error: Invalid or missing repo specified with \"-l\"";
			} else {
				strLocalRepo = itFlagServer->second[0];
			}
		} else {
			int iStatus = adm_getrepo_string(strLocalRepo);
			if (iStatus != 0) {
				return iStatus;
			}
		}

		if (strParseError != "") {
			printf("%s\nUsage: %s list [-l <local repo dir>]\n",
				strParseError.c_str(), strExecutable.c_str());
			return 1;
		}

		return adm_list(strLocalRepo);
	}

	// Remove data from the local repository
	if (strCommand == "remove") {
	}

	// Get data from the remote repository and store in the local repository
	if (strCommand == "get") {
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
		itFlagServer = mapFlags.find("l");
		if (itFlagServer != mapFlags.end()) {
			if (itFlagServer->second.size() != 1) {
				strParseError = "Error: Invalid or missing server name specified with \"-l\"";
			} else {
				strLocalRepo = itFlagServer->second[0];
			}
		} else {
			int iStatus = adm_getrepo_string(strLocalRepo);
			if (iStatus != 0) {
				return iStatus;
			}
		}

		return adm_get(strRemoteServer, strLocalRepo, strDataset);
	}

	// Put data on the server
	if (strCommand == "put") {
		//return adm_put();
	}
 
	} catch(Exception & e) {
		std::cout << e.ToString() << std::endl;
		return 1;

	} catch(...) {
		return 1;
	}

	return 0;
}
 
///////////////////////////////////////////////////////////////////////////////

