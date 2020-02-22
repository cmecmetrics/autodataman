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
#include "contrib/picosha2.h"

#include <string>
#include <vector>
#include <iostream>
#include <iomanip>
#include <fstream>
#include "filesystem_path.h"

#include "Exception.h"
#include "Namelist.h"

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
	///		Check for valid namelist variables.
	///	</summary>
	static bool IsValidVariable(
		const std::string & str
	) {
		if ((str == "tgz_open_command") ||
		    (str == "default_local_repo") ||
		    (str == "default_server")
		) {
			return true;
		}
		return false;
	}

public:
	///	<summary>
	///		Constructor.
	///	</summary>
	AutodatamanNamelist() :
		Namelist()
	{ }

protected:
	///	<summary>
	///		Set the default namelist variables.
	///	</summary>
	void SetDefault() {
		(*this)["tgz_open_command"] = "tar -xzf ";
	}

	///	<summary>
	///		Initialize the autodataman path.
	///	</summary>
	int InitializePath() {
		// Search for $HOME/.autodataman
		char * homedir = getenv("HOME");

		if (homedir != NULL) {
			filesystem::path pathNamelist(homedir);
			if (!pathNamelist.exists()) {
				_EXCEPTIONT("Environment variable $HOME points to an invalid home directory path\n");
				return 1;
			}
			m_path = pathNamelist/filesystem::path(".autodataman");
			return 0;
		}

		// Search for <pwd>/.autodataman
		uid_t uid = getuid();
		struct passwd *pw = getpwuid(uid);

		if (pw == NULL) {
			_EXCEPTIONT("Unable to identify path for .autodataman");
			return 1;
		}

		filesystem::path pathNamelist(pw->pw_dir);
		if (!pathNamelist.exists()) {
			_EXCEPTIONT("pwd points to an invalid home directory path");
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

		// If not populate default values
		} else {
			SetDefault();
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

///	<summary>
///		Break up dataset string into name and version.
///	</summary>
void get_dataset_name_version(
	const std::string & strDataset,
	std::string & strDatasetName,
	std::string & strDatasetVersion
) {
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
			strDatasetName = strDataset.substr(0, i);
			strDatasetVersion = strDataset.substr(i+1);
		}
	}
	if (strDatasetName == "") {
		strDatasetName = strDataset;
	}
}

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

int adm_config_get() {
	// Load .autodataman
	AutodatamanNamelist nml;
	int iStatus = nml.LoadFromUser();
	if (iStatus) return iStatus;

	printf("Configuration:\n");
	for (auto iter = nml.begin(); iter != nml.end(); iter++) {
		printf("  %s= %s\n", iter->first.c_str(), iter->second.c_str());
	}

	return 0;
}

///////////////////////////////////////////////////////////////////////////////

int adm_config_set(
	const std::string & strVariable,
	const std::string & strValue
) {
	if (!AutodatamanNamelist::IsValidVariable(strVariable)) {
		printf("Invalid config variable \"%s\"\n", strVariable.c_str());
		return 1;
	}

	AutodatamanNamelist nml;
	int iStatus = nml.LoadFromUser();
	if (iStatus) return iStatus;

	nml[strVariable] = strValue;

	iStatus = nml.SaveToUser();
	if (iStatus) return iStatus;

	return 0;
}

///////////////////////////////////////////////////////////////////////////////
 
int adm_initrepo(
	const std::string & strDir
) {
	filesystem::path pathRepo(strDir);

	// Check for repo path
	if (pathRepo.exists()) {
		_EXCEPTION1("Unable to create directory \"%s\": Specified path already exists",
			pathRepo.str().c_str());
		return 1;
	}

	// Create the directory
	bool fCreateDir = filesystem::create_directory(pathRepo);
	if (!fCreateDir) {
		_EXCEPTION1("Unable to create directory \"%s\": Failed in call to mkdir\n",
			pathRepo.str().c_str());
		return 1;
	}

	// Create a new JSON file in the directory
	AutodatamanRepoMD admmeta;

	filesystem::path pathMeta = pathRepo/filesystem::path("repo.txt");

	admmeta.to_file(pathMeta.str());

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

	printf("Connecting to server \"%s\".\n", strServer.c_str());

	AutodatamanRepoMD admrepo;
	admrepo.from_server(strServer);

	printf("Remote server contains %lu datasets.\n", admrepo.num_datasets());

	// Set this server to be default in .autodataman
	AutodatamanNamelist nml;
	int iStatus = nml.LoadFromUser();
	if (iStatus) return iStatus;

	nml["default_server"] = strServer;

	iStatus = nml.SaveToUser();
	if (iStatus) return iStatus;

	printf("Default autodataman server set to \"%s\"\n", strServer.c_str());

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
	const std::string & strServer,
	bool fVerbose
) {
	// Load repository descriptor from remote data server
	AutodatamanRepoMD admrepo;
	admrepo.from_server(strServer, fVerbose);

	// List available datasets
	if (admrepo.num_datasets() == 0) {
		printf("Server \"%s\" contains no datasets.\n",
			strServer.c_str());
		return 0;
	}
	
	if (!fVerbose) printf("Server \"%s\" contains %lu dataset(s)\n",
		strServer.c_str(), admrepo.num_datasets());

	const std::vector<std::string> & vecDatasets = admrepo.get_dataset_names();
	for (int i = 0; i < vecDatasets.size(); i++) {
		printf("  %s\n", vecDatasets[i].c_str());
	}

	return 0;
}

///////////////////////////////////////////////////////////////////////////////
 
int adm_list(
	const std::string & strLocalRepo,
	bool fVerbose
) {
	// Load repository descriptor from remote data server
	AutodatamanRepoMD admrepo;
	admrepo.from_local_repo(strLocalRepo);

	// List available datasets
	if (admrepo.num_datasets() == 0) {
		printf("Local repo \"%s\" contains no datasets\n", strLocalRepo.c_str());
		return 0;
	}

	printf("Local repo \"%s\" contains %lu dataset(s)\n",
		strLocalRepo.c_str(), admrepo.num_datasets());

	const std::vector<std::string> & vecDatasets = admrepo.get_dataset_names();
	for (int i = 0; i < vecDatasets.size(); i++) {

		// Load dataset metadata from local data server
		AutodatamanRepoDatasetMD admrepodataset;
		admrepodataset.from_local_repo(strLocalRepo, vecDatasets[i], fVerbose);

		// Display versions
		const std::vector<std::string> & vecVersions = admrepodataset.get_version_names();
		for (int j = 0; j < vecVersions.size(); j++) {
			printf("  %s/%s\n", vecDatasets[i].c_str(), vecVersions[j].c_str());
		}
		if (vecVersions.size() == 0) {
			printf("  %s (0 versions)\n", vecDatasets[i].c_str());
		}
	}

	return 0;
}

///////////////////////////////////////////////////////////////////////////////
 
int adm_info(
	const std::string & strServer,
	const std::string & strLocalRepo,
	const std::string & strDataset,
	bool fVerbose
) {
	printf("== SERVER COPY ==============================\n");
	printf("Server \"%s\"\n", strServer.c_str());

	// Load repository metadata from remote data server
	AutodatamanRepoMD admserver;
	admserver.from_server(strServer, fVerbose);

	// Verify requested dataset exists in the repository
	int iServerDataset = admserver.find_dataset(strDataset);
	if (iServerDataset == (-1)) {
		printf("Dataset \"%s\" not found on remote server\n",
			strDataset.c_str());

	} else {
		// Load dataset metadata from remote data server
		AutodatamanRepoDatasetMD admrepodataset;
		admrepodataset.from_server(strServer, strDataset, fVerbose);

		// Output information
		printf("Long name:  %s\n", admrepodataset.get_long_name().c_str());
		printf("Short name: %s\n", admrepodataset.get_short_name().c_str());
		if (admrepodataset.get_source().length() != 0) {
			printf("Source:     %s\n", admrepodataset.get_source().c_str());
		} else {
			printf("Source:     (unknown)\n");
		}
		printf("Version(s) available:\n");

		// List available datasets
		const std::vector<std::string> & vecVersions =
			admrepodataset.get_version_names();
		for (int i = 0; i < vecVersions.size(); i++) {
			if (vecVersions[i] == admrepodataset.get_default_version()) {
				printf("  %s [default]\n", vecVersions[i].c_str());
			} else {
				printf("  %s\n", vecVersions[i].c_str());
			}
		}
	}

	printf("== LOCAL COPY ===============================\n");
	printf("Local repo \"%s\"\n", strLocalRepo.c_str());

	// Load repository metadata from local data server
	AutodatamanRepoMD admlocal;
	admlocal.from_local_repo(strLocalRepo, fVerbose);

	int iLocalDataset = admlocal.find_dataset(strDataset);
	if (iLocalDataset == (-1)) {
		printf("Dataset \"%s\" not found in local repo\n",
			strDataset.c_str());

	} else {
		// Load dataset metadata from local data server
		AutodatamanRepoDatasetMD admrepodataset;
		admrepodataset.from_local_repo(strLocalRepo, strDataset, fVerbose);

		// Output information
		printf("Long name:  %s\n", admrepodataset.get_long_name().c_str());
		printf("Short name: %s\n", admrepodataset.get_short_name().c_str());
		if (admrepodataset.get_source().length() != 0) {
			printf("Source:     %s\n", admrepodataset.get_source().c_str());
		} else {
			printf("Source:     (unknown)\n");
		}
		printf("Version(s) available:\n");

		// List available datasets
		const std::vector<std::string> & vecVersions =
			admrepodataset.get_version_names();
		for (int i = 0; i < vecVersions.size(); i++) {
			if (vecVersions[i] == admrepodataset.get_default_version()) {
				printf("  %s [default]\n", vecVersions[i].c_str());
			} else {
				printf("  %s\n", vecVersions[i].c_str());
			}
		}
		if (vecVersions.size() == 0) {
			printf("  (none)\n");
		}
		printf("=============================================\n");
	}

	return 0;
}

///////////////////////////////////////////////////////////////////////////////
 
int adm_remove(
	const std::string & strLocalRepo,
	const std::string & strDataset,
	bool fRemoveAll,
	bool fVerbose
) {
	// Check arguments
	if (strLocalRepo.length() == 0) {
		_EXCEPTIONT("Missing local repo path\n");
	}
	if (strDataset.length() == 0) {
		_EXCEPTIONT("Missing dataset name\n");
	}

	// Repository information
	printf("Local repo \"%s\"\n", strLocalRepo.c_str());

	// Break up dataset into name and version
	std::string strDatasetName;
	std::string strDatasetVersion;
	get_dataset_name_version(strDataset, strDatasetName, strDatasetVersion);

	// Path to repo
	filesystem::path pathRepo = filesystem::path(strLocalRepo);
	if (pathRepo.str().length() < 2) {
		_EXCEPTION1("Invalid local repository name \"%s\"", pathRepo.str().c_str());
	}

	// Get the local repo metadata
	AutodatamanRepoMD admlocalrepo;
	admlocalrepo.from_local_repo(strLocalRepo, fVerbose);

	int iLocalDataset = admlocalrepo.find_dataset(strDatasetName);
	if (iLocalDataset == (-1)) {
		printf("Dataset \"%s\" not found in local repo\n",
			strDatasetName.c_str());
		return 1;
	}

	// Path to dataset
	filesystem::path pathDataset = pathRepo/filesystem::path(strDatasetName);

	if (!pathDataset.exists()) {
		_EXCEPTION1("Damaged local repo.  Path \"%s\" does not exist in repo, "
			"but is referenced in repo metadata.  Try running \"repair\" on repo.",
			pathDataset.str().c_str());
	}

	if (!pathDataset.is_directory()) {
		_EXCEPTION1("Damaged local repo.  Path \"%s\" is not a directory, "
			"but is referenced in repo metadata.  Try running \"repair\" on repo.",
			pathDataset.str().c_str());
	}

	// Load dataset metadata
	AutodatamanRepoDatasetMD admlocaldataset;
	admlocaldataset.from_local_repo(strLocalRepo, strDatasetName, fVerbose);

	// No version specified; try to remove entire dataset
	if (strDatasetVersion == "") {
		if (admlocaldataset.num_versions() == 0) {
			fRemoveAll = true;
			printf("Removing dataset \"%s\" (0 versions)\n",
				strDatasetName.c_str());
		}
		if (admlocaldataset.num_versions() == 1) {
			fRemoveAll = true;
			printf("Removing dataset \"%s\" (1 version)\n",
				strDatasetName.c_str());
		}

		if (!fRemoveAll) {
			printf("Dataset \"%s\" contains multiple versions (%lu).\n"
				"To remove entire dataset rerun with \"-a\".\n",
				strDatasetName.c_str(), admlocaldataset.num_versions());
			return 1;

		} else {
			admlocalrepo.remove_dataset(strDatasetName);

			std::string strCommand =
				std::string("rm -rf ") + pathDataset.str();
			if (fVerbose) printf("Executing \"%s\"\n", strCommand.c_str());

			if (strCommand.length() < pathRepo.str().length() + 6) {
				_EXCEPTIONT("Failsafe triggered:  For safety reasons, "
					"aborting execution of command.");
			}

			int iResult = std::system(strCommand.c_str());
			if (iResult != 0) {
				_EXCEPTIONT("Command failed.  Aborting.");
			}
		}

	// Version specified
	} else {

		// Check that version exists in metadata
		int iLocalVersion = admlocaldataset.find_version(strDatasetVersion);
		if (iLocalVersion == (-1)) {
			printf("Version \"%s\" not found in local dataset \"%s\"\n",
				strDatasetVersion.c_str(), strDatasetName.c_str());
			return 1;
		}

		// Remove from metadata
		admlocaldataset.remove_version(strDatasetVersion);

		// Path to version
		filesystem::path pathVersion = pathDataset/filesystem::path(strDatasetVersion);

		if (!pathVersion.exists()) {
			_EXCEPTION1("Damaged local repo.  Path \"%s\" does not exist in repo, "
				"but is referenced in dataset metadata.  Try running \"repair\" on repo.",
				pathVersion.str().c_str());
		}

		if (!pathDataset.is_directory()) {
			_EXCEPTION1("Damaged local repo.  Path \"%s\" is not a directory, "
				"but is referenced in dataset metadata.  Try running \"repair\" on repo.",
				pathVersion.str().c_str());
		}

		// Execute removal of version directory
		std::string strCommand =
			std::string("rm -rf ") + pathVersion.str();
		if (fVerbose) printf("Executing \"%s\"\n", strCommand.c_str());
		if (strCommand.length() < pathRepo.str().length() + 6) {
			_EXCEPTIONT("Failsafe triggered:  For safety reasons, "
				"aborting execution of command.");
		}

		int iResult = std::system(strCommand.c_str());
		if (iResult != 0) {
			_EXCEPTIONT("Command failed.  Aborting.");
		}
	}

	/////////////////////////////////////////////////////////////////////////////
	// If these commands trigger an exception we've likely corrupted the repo
	try {
		// Write updated repo metadata
		if (strDatasetVersion == "") {
			filesystem::path pathRepoMetaJSON = pathRepo/filesystem::path("repo.txt");
			if (fVerbose) printf("Writing repo metadata to \"%s\" (contains %lu datasets)\n",
				pathRepoMetaJSON.str().c_str(), admlocalrepo.num_datasets());
			admlocalrepo.to_file(pathRepoMetaJSON.str());
		}

		// Write updated dataset metadata
		if (strDatasetVersion != "") {
			filesystem::path pathDatasetMetaJSON = pathDataset/filesystem::path("dataset.txt");
			if (fVerbose) printf("Writing dataset metadata to \"%s\" (contains %lu versions)\n",
				pathDatasetMetaJSON.str().c_str(),
				admlocaldataset.num_versions());
			admlocaldataset.to_file(pathDatasetMetaJSON.str());
		}

	} catch(Exception & e) {
		printf("DANGER: Exception may have corrupted repository.  "
			"Run \"validate\" to check.\n");
		throw std::runtime_error(e.ToString());

	} catch(std::runtime_error & e) {
		printf("DANGER: Exception may have corrupted repository.  "
			"Run \"validate\" to check.\n");
		throw e;

	} catch(...) {
		printf("DANGER: Exception may have corrupted repository.  "
			"Run \"validate\" to check.\n");
		throw std::runtime_error("Unknown exception");
	}

	// Success
	printf("Dataset \"%s\" removed successfully\n", strDataset.c_str());

	return 0;
}

///////////////////////////////////////////////////////////////////////////////
 
int adm_get(
	const std::string & strServer,
	const std::string & strLocalRepo,
	const std::string & strDataset,
	bool fForceOverwrite,
	bool fVerbose
) {
	// Check arguments
	if (strServer.length() == 0) {
		_EXCEPTIONT("Missing server url\n");
	}
	if (strLocalRepo.length() == 0) {
		_EXCEPTIONT("Missing local repo path\n");
	}
	if (strDataset.length() == 0) {
		_EXCEPTIONT("Missing dataset name\n");
	}

	// Output arguments
	printf("Local repo \"%s\"\n", strLocalRepo.c_str());
	printf("Server \"%s\"\n", strServer.c_str());

	// Break up dataset into name and version
	std::string strDatasetName;
	std::string strDatasetVersion;
	get_dataset_name_version(strDataset, strDatasetName, strDatasetVersion);

	// Path to repo
	filesystem::path pathRepo = filesystem::path(strLocalRepo);
	if (pathRepo.str().length() < 2) {
		_EXCEPTION1("Invalid local repository name \"%s\"", pathRepo.str().c_str());
	}

	// Path to dataset
	filesystem::path pathDataset = pathRepo/filesystem::path(strDatasetName);

	// Flag indicating that this is a new dataset
	bool fNewDataset = false;

	// Flag indicating that the current version already exists, should be overwritten
	bool fOverwriteVersion = false;

	////////////////////////////////////////////////////
	// Load repository descriptor from remote data server
	AutodatamanRepoMD admserver;
	admserver.from_server(strServer, fVerbose);

	// Verify requested dataset exists in the repository
	int iServerDataset = admserver.find_dataset(strDatasetName);
	if (iServerDataset == (-1)) {
		printf("Dataset \"%s\" not found on remote server\n",
			strDatasetName.c_str());
		return 1;
	}

	// Load dataset descriptor from remote data server
	AutodatamanRepoDatasetMD admserverdataset;
	admserverdataset.from_server(strServer, strDatasetName, fVerbose);

	// Use default version number, if not specified
	if (strDatasetVersion == "") {
		strDatasetVersion = admserverdataset.get_default_version();
		if (strDatasetVersion == "") {
			printf("No default version of dataset \"%s\" found. "
				"Please specify a version:\n", strDatasetName.c_str());
			const std::vector<std::string> & vecVersions =
				admserverdataset.get_version_names();
			for (size_t i = 0; i < vecVersions.size(); i++) {
				printf("  %s\n", vecVersions[i].c_str());
			}
			return 1;
		}
		printf("Default dataset version is \"%s\".\n", strDatasetVersion.c_str());
	}

	// Verify requested version exists in the repository
	int iServerVersion = admserverdataset.find_version(strDatasetVersion);
	if (iServerVersion == (-1)) {
		printf("Dataset \"%s\" version \"%s\" not found on remote server.\n",
			strDatasetName.c_str(), strDatasetVersion.c_str());
		return 1;
	}

	// Download data descriptor from remote data server
	AutodatamanRepoDataMD admserverdata;
	admserverdata.from_server(strServer, strDatasetName, strDatasetVersion, fVerbose);

	// Load repository descriptor from local data server
	AutodatamanRepoMD admlocalrepo;
	admlocalrepo.from_local_repo(strLocalRepo, fVerbose);

	// Path to version
	filesystem::path pathVersion = pathDataset/filesystem::path(strDatasetVersion);

	// Path to directory storing downloaded data
	// This is the same as pathVersion unless fOverwriteVersion is true
	filesystem::path pathVersionTemp = pathVersion;

	////////////////////////////////////////////////////
	// Check if dataset exists in the local repository
	AutodatamanRepoDatasetMD admlocaldataset;

	int iLocalDataset = admlocalrepo.find_dataset(strDatasetName);

	// Text of exception if code aborts
	std::string strAbortText;

	// Catch all exceptions from here so we can clean up data directory
	try {

		// Dataset does not exist in local repository
		if (iLocalDataset == (-1)) {

			// Check for existence of dataset directory
			if (pathDataset.exists()) {
				_EXCEPTION1("Damaged local repo.  Path \"%s\" already exists in repo, "
					"but not referenced in repo metadata.  Try running \"repair\" on repo.",
					pathDataset.str().c_str());
			}

			// Create dataset directory
			bool fSuccess = filesystem::create_directory(pathDataset);
			if (!fSuccess) {
				_EXCEPTION1("Unable to create directory \"%s\"",
					pathDataset.str().c_str());
			}

			// New dataset
			fNewDataset = true;

			// Copy metadata
			admlocaldataset.set_from_admdataset(admserverdataset);

			// Add to repo
			admlocalrepo.add_dataset(strDatasetName);

		// Dataset exists in local repository
		} else {

			// Check for existence of dataset path
			if (!pathDataset.exists()) {
				_EXCEPTION1("Damaged local repo.  Path \"%s\" does not exist in repo, "
					"but is referenced in repo metadata.  Try running \"repair\" on repo.",
					pathDataset.str().c_str());
			}

			// Verify dataset path is a directory
			if (!pathDataset.is_directory()) {
				_EXCEPTION1("Damaged local repo.  Path \"%s\" is not a directory, "
					"but is referenced in repo metadata.  Try running \"repair\" on repo.",
					pathDataset.str().c_str());
			}

			// Load the metadata file
			admlocaldataset.from_local_repo(strLocalRepo, strDatasetName, fVerbose);
		}

		////////////////////////////////////////////////////
		// Check if version exists in the local repository
		AutodatamanRepoDataMD admlocaldata;

		int iLocalVersion = admlocaldataset.find_version(strDatasetVersion);

		// Version does not exist in dataset
		if (iLocalVersion == (-1)) {

			// Check for existence of version directory
			if (pathVersionTemp.exists()) {
				_EXCEPTION1("Damaged local repo.  Path \"%s\" already exists in repo, "
					"but not referenced in repo metadata.  Try running \"repair\" on repo.",
					pathVersionTemp.str().c_str());
			}

			// Add to dataset
			admlocaldataset.add_version(strDatasetVersion);

		// Version exists in dataset
		} else {
			// Load the local metadata file
			admlocaldata.from_local_repo(
				strLocalRepo, strDatasetName, strDatasetVersion, fVerbose);

			// Check for equivalence
			if (admserverdata == admlocaldata) {
				printf("Specified dataset \"%s\" already exists on local repo.\n",
					strDataset.c_str());

				if (!fForceOverwrite) {
					printf("Rerun with \"-f\" to force overwrite.\n");
					return 0;
				} else {
					printf("Overwriting with server data.\n");
				}

			} else {
				printf("== SERVER COPY ==============================\n");
				admserverdata.summary();
				printf("== LOCAL COPY ===============================\n");
				admlocaldata.summary();
				printf("=============================================\n");
				printf("WARNING: Specified dataset \"%s\" exists on local repo, "
					"but metadata descriptor does not match.  This could mean "
					"that one or both datasets are corrupt.  ",
					strDataset.c_str());

				if (!fForceOverwrite) {
					printf("Rerun with \"-f\" to force overwrite.\n");
					return 0;
				} else {
					printf("Overwriting with server data.\n");
				}
			}

			// Set temporary path for storing data
			pathVersionTemp = pathDataset/filesystem::path(strDatasetVersion + ".part");

			// Overwrite
			fOverwriteVersion = true;
		}

		// Local metadata will be overwritten by server metadata
		admlocaldata = admserverdata;

		// Create data directory
		bool fSuccess = filesystem::create_directory(pathVersionTemp);
		if (!fSuccess) {
			_EXCEPTION1("Unable to create directory \"%s\".  Try running \"repair\" on repo.",
				pathVersionTemp.str().c_str());
		}

		// Write metadata
		filesystem::path pathDataMetaJSON =
			pathVersionTemp/filesystem::path("data.txt");

		if (fVerbose)
			printf("Writing version metadata to \"%s\"\n",
				pathDataMetaJSON.str().c_str());

		admlocaldata.to_file(pathDataMetaJSON.str());

		////////////////////////////////////////////////////
		// Download data
		printf("=============================================\n");
		std::string strRemoteFilePath = strServer;
		_ASSERT(strRemoteFilePath.length() > 0);
		if (strRemoteFilePath[strRemoteFilePath.length()-1] != '/') {
			strRemoteFilePath += "/";
		}
		strRemoteFilePath += strDatasetName + "/" + strDatasetVersion + "/";

		for (int i = 0; i < admlocaldata.num_files(); i++) {
			const AutodatamanRepoFileMD & admlocalfile = admlocaldata[i];

			std::string strRemoteFile =
				strRemoteFilePath + admlocalfile.get_filename();

			filesystem::path pathFile =
				pathVersionTemp/filesystem::path(admlocalfile.get_filename());

			printf("Downloading \"%s\"\n", admlocalfile.get_filename().c_str());
			if (fVerbose) printf("Local target \"%s\"\n", pathFile.str().c_str());

			curl_download_file(strRemoteFile, pathFile.str());

			// Check SHA256
			std::ifstream ifDownload(pathFile.str(), std::ios::binary);
			if (!ifDownload.is_open()) {
				_EXCEPTION1("ERROR: Unable to open downloaded file \"%s\". Aborting.",
					pathFile.str().c_str());
			}
			std::vector<unsigned char> vecsha(picosha2::k_digest_size);
			picosha2::hash256(ifDownload, vecsha.begin(), vecsha.end());
			std::string strSHA256 =
				picosha2::bytes_to_hex_string(vecsha.begin(), vecsha.end());

			if (strSHA256 != admlocalfile.get_sha256sum()) {
				printf("Repository SHA256 %s\n", admlocalfile.get_sha256sum().c_str());
				_EXCEPTION2("ERROR: Failed to verify file SHA256. If local data "
					"repository is inconsistent try \"remove %s/%s\" before "
					"downloading again.",
					strDatasetName.c_str(), strDatasetVersion.c_str());
			} else {
				printf("Verified SHA256 %s\n", strSHA256.c_str());
			}
		}
		printf("=============================================\n");

		////////////////////////////////////////////////////
		// Apply OnDownload operations
		
		// Load the namelist
		AutodatamanNamelist nml;
		int iStatus = nml.LoadFromUser();
		if (iStatus) return iStatus;

		// Apply system commands
		bool fHasOnDownload = false;
		for (int i = 0; i < admlocaldata.num_files(); i++) {
			const AutodatamanRepoFileMD & admlocalfile = admlocaldata[i];

			filesystem::path pathFile =
				pathVersionTemp/filesystem::path(admlocalfile.get_filename());

			if (admlocalfile.get_ondownload() != "") {
				fHasOnDownload = true;

				std::string strNamelistVar =
					admlocalfile.get_format() + "_"
					+ admlocalfile.get_ondownload() + "_command";

				std::string strCommand =
					std::string("cd ") + pathVersionTemp.str() + " && "
					+ nml[strNamelistVar] + " " + pathFile.str()
					+ " && rm " + pathFile.str();

				if (strCommand != "") {
					printf("Executing \"%s\"\n", strCommand.c_str());
					int iResult = std::system(strCommand.c_str());
					if (iResult != 0) {
						_EXCEPTIONT("Command failed.  Aborting.");
					}
				}
			}
		}

		if (fHasOnDownload) {
			printf("=============================================\n");
		}

	// Exception occurred; clean-up
	} catch(Exception & e) {
		strAbortText = e.ToString();
	} catch(std::runtime_error & e) {
		strAbortText = e.what();
	} catch(...) {
		strAbortText = "Unknown exception";
	}

	////////////////////////////////////////////////////
	// Cleanup if an exception occurs
	if (strAbortText != "") {

		if (fNewDataset || fOverwriteVersion) {
			printf("Exception caused code to abort.  Cleaning up.\n");
		}

		// Remove dataset directory created earlier in this function
		if (fNewDataset) {
			if (pathDataset.exists()) {
				std::string strCommand = std::string("rm -rf ") + pathDataset.str();
				printf("..Executing \"%s\"\n", strCommand.c_str());
				if (strCommand.length() < pathRepo.str().length() + 6) {
					_EXCEPTIONT("Failsafe triggered:  For safety reasons, aborting execution of command.");
				}

				std::system(strCommand.c_str());
			}

		// Remove version directory created earlier in this function
		} else if (fOverwriteVersion) {
			if (pathVersionTemp.exists()) {
				std::string strCommand = std::string("rm -rf ") + pathVersionTemp.str();
				printf("..Executing \"%s\"\n", strCommand.c_str());
				if (strCommand.length() < pathRepo.str().length() + 6) {
					_EXCEPTIONT("Failsafe triggered:  For safety reasons, aborting execution of command.");
				}

				std::system(strCommand.c_str());
			}
		}

		throw std::runtime_error(strAbortText.c_str());
	}

	/////////////////////////////////////////////////////////////////////////////
	// If these commands trigger an exception we've likely corrupted the repo
	try {

		// Keep old dataset metadata, but rename version directory
		if (fOverwriteVersion) {

			// Remove old version directory
			{
				std::string strCommand = std::string("rm -rf ") + pathVersion.str();
				printf("Executing \"%s\"\n", strCommand.c_str());
				if (strCommand.length() < pathRepo.str().length() + 6) {
					_EXCEPTIONT("Failsafe triggered:  For safety reasons, "
						"aborting execution of command.");
				}

				int iResult = std::system(strCommand.c_str());
				if (iResult != 0) {
					_EXCEPTIONT("Command failed.  Aborting.");
				}
			}

			// Move new version directory to old location
			{
				std::string strCommand =
					std::string("mv ") + pathVersionTemp.str()
					+ " " + pathVersion.str();
				printf("Executing \"%s\"\n", strCommand.c_str());

				int iResult = std::system(strCommand.c_str());
				if (iResult != 0) {
					_EXCEPTIONT("Command failed.  Aborting.");
				}
			}

		// Write updated dataset metadata
		} else {
			filesystem::path pathDatasetMetaJSON = pathDataset/filesystem::path("dataset.txt");
			if (fVerbose) printf("Writing dataset metadata to \"%s\" (contains %lu versions)\n",
				pathDatasetMetaJSON.str().c_str(),
				admlocaldataset.num_versions());
			admlocaldataset.to_file(pathDatasetMetaJSON.str());
		}

		// Write updated repo metadata
		if (fNewDataset) {
			filesystem::path pathRepoMetaJSON = pathRepo/filesystem::path("repo.txt");
			if (fVerbose) printf("Writing repo metadata to \"%s\" (contains %lu datasets)\n",
				pathRepoMetaJSON.str().c_str(),
				admlocalrepo.num_datasets());
			admlocalrepo.to_file(pathRepoMetaJSON.str());
		}

	} catch(Exception & e) {
		printf("DANGER: Exception may have corrupted repository.  "
			"Run \"validate\" to check.\n");
		throw std::runtime_error(e.ToString());

	} catch(std::runtime_error & e) {
		printf("DANGER: Exception may have corrupted repository.  "
			"Run \"validate\" to check.\n");
		throw e;

	} catch(...) {
		printf("DANGER: Exception may have corrupted repository.  "
			"Run \"validate\" to check.\n");
		throw std::runtime_error("Unknown exception");
	}

	// Success
	printf("Dataset \"%s\" retrieved successfully\n", strDataset.c_str());

	return 0;
}

///////////////////////////////////////////////////////////////////////////////

int main(int argc, char **argv) {

	try {

	// Executable
	std::string strExecutable = argv[0];

	// Command
	std::string strCommand;
	if (argc >= 2) {
		strCommand = argv[1];
	}

	// Arguments
	std::vector<std::string> vecArg;
	for (int c = 2; c < argc; c++) {
		vecArg.push_back(argv[c]);
	}

	// Configure
	if (strCommand == "config") {
		if (argc == 2) {
			return adm_config_get();

		} else if (argc == 4) {
			std::string strVariable = argv[2];
			std::string strValue = argv[3];

			return adm_config_set(strVariable, strValue);

		} else {
			printf("Usage: %s config [<variable> <value>]\n", strExecutable.c_str());
			return 1;
		}
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
		spec["v"] = 0;

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

		bool fVerbose = (mapFlags.find("v") != mapFlags.end());

		return adm_avail(strRemoteServer, fVerbose);
	}

	// Check for data information
	if (strCommand == "info") {
		CommandLineFlagSpec spec;
		spec["s"] = 1;
		spec["l"] = 1;
		spec["v"] = 0;

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
			printf("%s\nUsage: %s info [-v] [-l <local repo dir>] [-s <server>] <dataset id>\n",
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

		bool fVerbose = (mapFlags.find("v") != mapFlags.end());

		return adm_info(strRemoteServer, strLocalRepo, strDataset, fVerbose);
	}

	// Check for data on the local repository
	if (strCommand == "list") {
		CommandLineFlagSpec spec;
		spec["l"] = 1;
		spec["v"] = 0;

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

		bool fVerbose = (mapFlags.find("v") != mapFlags.end());

		return adm_list(strLocalRepo, fVerbose);
	}

	// Remove data from the local repository
	if (strCommand == "remove") {
		CommandLineFlagSpec spec;
		spec["l"] = 1;
		spec["a"] = 0;
		spec["v"] = 0;

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
			printf("%s\nUsage: %s remove [-v] [-l <local repo dir>] <dataset id>[/<version>]\n",
				strParseError.c_str(), strExecutable.c_str());
			return 1;
		}

		std::string strLocalRepo;
		auto itFlagServer = mapFlags.find("l");
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

		bool fRemoveAll = (mapFlags.find("a") != mapFlags.end());

		bool fVerbose = (mapFlags.find("v") != mapFlags.end());

		return adm_remove(strLocalRepo, strDataset, fRemoveAll, fVerbose);

	}

	// Get data from the remote repository and store in the local repository
	if (strCommand == "get") {
		CommandLineFlagSpec spec;
		spec["s"] = 1;
		spec["l"] = 1;
		spec["f"] = 0;
		spec["v"] = 0;

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
			printf("%s\nUsage: %s get [-f] [-v] [-l <local repo dir>] [-s <server>] <dataset id>[/<version>]\n",
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

		bool fForceOverwrite = (mapFlags.find("f") != mapFlags.end());

		bool fVerbose = (mapFlags.find("v") != mapFlags.end());

		return adm_get(strRemoteServer, strLocalRepo, strDataset, fForceOverwrite, fVerbose);
	}

	// Put data on the server
	if (strCommand == "put") {
		//return adm_put();
	}
 
	// Check command line arguments
	{
		printf("Usage:\n");
		printf("%s config [<variable> <value>]\n", strExecutable.c_str());
		printf("%s initrepo <local repo dir>\n", strExecutable.c_str());
		printf("%s setrepo <local repo dir>\n", strExecutable.c_str());
		printf("%s getrepo\n", strExecutable.c_str());
		printf("%s setserver <server>\n", strExecutable.c_str());
		printf("%s getserver\n", strExecutable.c_str());
		printf("%s avail [-v] [-s <server>]\n", strExecutable.c_str());
		printf("%s info [-v] [-l <local repo dir>] [-s <server>] <dataset id>\n", strExecutable.c_str());
		printf("%s list [-v] [-l <local repo dir>]\n", strExecutable.c_str());
		printf("%s remove [-l <local repo dir>] [-s <server>] <dataset id>[/version]\n", strExecutable.c_str());
		printf("%s get [-f] [-v] [-l <local repo dir>] [-s <server>] <dataset id>[/version]\n", strExecutable.c_str());
		printf("%s put [-l <local repo dir>] [-s <server>] [-c <compression type>] [-v <version>] <dataset dir>\n", strExecutable.c_str());
		return 1;
	}

	} catch(Exception & e) {
		std::cout << e.ToString() << std::endl;
		return 1;

	} catch(std::runtime_error & e) {
		std::cout << e.what() << std::endl;
		return 1;

	} catch(...) {
		return 1;
	}

	return 0;
}
 
///////////////////////////////////////////////////////////////////////////////

