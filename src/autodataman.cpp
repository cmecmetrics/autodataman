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

#include "Repository.h"
#include "Namelist.h"

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
	// Repo metadata
	std::string strServerMetadata = strServer;
	if (strServer[strServer.length()-1] == '/') {
		strServerMetadata += "repo.txt";
	} else {
		strServerMetadata += "/repo.txt";
	}

	// Notify user
	printf("Attempting to download server metadata file \"%s\"\n",
		strServerMetadata.c_str());

	// Verify server exists
	CURL *curl_handle;
 
	curl_global_init(CURL_GLOBAL_ALL);
 
	// init the curl session
	curl_handle = curl_easy_init();
 
	// set URL to get here
	curl_easy_setopt(curl_handle, CURLOPT_URL, strServerMetadata.c_str());
 
	// Switch on full protocol/debug output while testing
	curl_easy_setopt(curl_handle, CURLOPT_VERBOSE, 0L);
 
	// disable progress meter, set to 0L to enable it
	curl_easy_setopt(curl_handle, CURLOPT_NOPROGRESS, 1L);
 
	// send all data to this function
	curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, write_data_to_stringstream);
 
	// open the file
	std::stringstream strFile;
 
	// write the page body to this string stream
	curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, (void*)(&strFile));
 
	// get it!
	curl_easy_perform(curl_handle);
 
	// cleanup curl stuff
	curl_easy_cleanup(curl_handle);
 
	curl_global_cleanup();

	printf("Download completed.  Parsing metadata file.\n");

	std::cout << "=============================================" << std::endl;
	std::cout << strFile.str() << std::endl;
	std::cout << "=============================================" << std::endl;

	// Verify server is a valid repository
	try {
		nlohmann::json::parse(strFile.str());
	} catch (nlohmann::json::parse_error& e)
    {
		std::cout << "ERROR parsing server repo metadata" << std::endl;
        std::cout << "  message: " << e.what() << '\n'
                  << "  exception id: " << e.id << '\n'
                  << "  byte position of error: " << e.byte << std::endl;

		std::cout << "FAILED to set default server" << std::endl;
		return 1;
    }

	printf("Parsing complete.  Validating server contents.\n");

	printf("WARNING: Validation not implemented...\n");

	// Set this server to be default in .autodataman
	AutodatamanNamelist nml;
	int iStatus = nml.LoadFromUser();
	if (iStatus) return iStatus;

	nml["default_server"] = strServer;

	iStatus = nml.SaveToUser();
	if (iStatus) return iStatus;

	printf("SUCCESSFULLY set autodataman server to \"%s\"\n", strServer.c_str());

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
 
int adm_avail() {
	// Load repository descriptor from remote data server

	// List contents of the repository

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

	} catch(...) {
		return 1;
	}

	return 0;
}
 
///////////////////////////////////////////////////////////////////////////////

