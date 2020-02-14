///////////////////////////////////////////////////////////////////////////////
///
///	\file    admcurl.h
///	\author  Paul Ullrich
///	\version February 10, 2020
///
///	<remarks>
///		Copyright (c) 2020 Paul Ullrich 
///	
///		Distributed under the Boost Software License, Version 1.0.
///		(See accompanying file LICENSE_1_0.txt or copy at
///		http://www.boost.org/LICENSE_1_0.txt)
///	</remarks>

#ifndef _ADMCURL_H_
#define _ADMCURL_H_

#include <curl/curl.h>

#include <string>
#include <sstream>
#include <iomanip>
#include <cstdlib>

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

static size_t write_data_to_file(
	void *ptr,
	size_t size,
	size_t nmemb,
	void *stream
) {
  size_t written = fwrite(ptr, size, nmemb, (FILE *)stream);
  return written;
}

///////////////////////////////////////////////////////////////////////////////

int curl_download_file(
	const std::string & strURL,
	const std::string & strFilename
) {
	CURL *curl_handle;
	FILE *pagefile;

	curl_global_init(CURL_GLOBAL_ALL);
 
	// init the curl session
	curl_handle = curl_easy_init();
 
	// set URL to get here
	curl_easy_setopt(curl_handle, CURLOPT_URL, strURL.c_str());
 
	// Switch on full protocol/debug output while testing
	//curl_easy_setopt(curl_handle, CURLOPT_VERBOSE, 0L);
 
	// disable progress meter, set to 0L to enable it
	curl_easy_setopt(curl_handle, CURLOPT_NOPROGRESS, 0L);
 
	// send all data to this function
	curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, write_data_to_file);
 
	// open the file
	pagefile = fopen(strFilename.c_str(), "wb");
	if (pagefile) {
 
		// write the page body to this file handle
		curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, pagefile);
 
		// get it!
		curl_easy_perform(curl_handle);
 
		// close the header file
		fclose(pagefile);

	} else {
		return 1;
	}
 
	// cleanup curl stuff
	curl_easy_cleanup(curl_handle);
 
	curl_global_cleanup();

	return 0;
}

///////////////////////////////////////////////////////////////////////////////

#endif // _ADMCURL_H_

