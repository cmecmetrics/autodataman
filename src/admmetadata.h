///////////////////////////////////////////////////////////////////////////////
///
///	\file    admmetadata.h
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

#ifndef _ADMMETADATA_H_
#define _ADMMETADATA_H_

#include <string>
#include <vector>
#include <fstream>
#include <iostream>

#include "Exception.h"
#include "contrib/json.hpp"
#include "admcurl.h"

///////////////////////////////////////////////////////////////////////////////

class AutodatamanRepoMD {

public:
	///	<summary>
	///		Constructor.
	///	</summary>
	AutodatamanRepoMD() {
		m_strVersion = version();
	}

	///	<summary>
	///		Get the version number.
	///	</summary>
	static std::string version() {
		return std::string("2020-02-13 (v0.1)");
	}

	///	<summary>
	///		Clear the repo.
	///	</summary>
	void clear() {
		m_strVersion = version();
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

		printf("Repository contains %lu dataset(s)\n", m_vecDatasetNames.size());

		return 0;
	}

	///	<summary>
	///		Construct a JSON object from this object.
	///	</summary>
	void to_JSON(
		nlohmann::json & jmeta
	) {
		jmeta["_REPO"]["type"] = "autodataman";
		jmeta["_REPO"]["version"] = "2020-02-13 (v0.1)";
		auto jsonFiles = jmeta["_DATASETS"] = nlohmann::json::array();
		for (auto i = 0; i < m_vecDatasetNames.size(); i++) {
			jsonFiles.push_back(m_vecDatasetNames[i]);
		}
	}

	///	<summary>
	///		Write the metadata file.
	///	</summary>
	void to_file(
		const std::string & strFile
	) {
		std::ofstream ofs(strFile);
		if (!ofs.is_open()) {
			_EXCEPTION1("Unable to open file \"%s\" for writing", strFile.c_str());
		}
		nlohmann::json jmeta;
		to_JSON(jmeta);
		ofs << jmeta;
		ofs.close();
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
		printf("Displaying information for server \"%s\"\n", strServer.c_str());
 
		// Download the metadata file from the server
		std::stringstream strFile;
		curl_download_to_stringstream(strServerMetadata, strFile);

		// Parse into a JSON object
		printf("Parsing server metadata file.\n");

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

		printf("Validating and deserializing metadata.\n");

		return from_JSON(jmeta);
	}

	///	<summary>
	///		Populate from local repository.
	///	</summary>
	int from_local_repo(
		const std::string & strRepoPath
	) {
		// Repo metadata
		std::string strRepoMetadataPath = strRepoPath;
		if (strRepoMetadataPath[strRepoMetadataPath.length()-1] != '/') {
			strRepoMetadataPath += "/";
		}
		strRepoMetadataPath += "repo.txt";

		std::ifstream ifs(strRepoMetadataPath);
		if (!ifs.is_open()) {
			_EXCEPTION1("Unable to open repository metadata file \"%s\"",
				strRepoMetadataPath.c_str());
		}
		nlohmann::json jmeta;
		try {
			jmeta = nlohmann::json::parse(ifs);
		} catch (nlohmann::json::parse_error& e) {
			_EXCEPTION3("Malformed repository metadata file "
				"%s (%i) at byte position %i",
				e.what(), e.id, e.byte);
		}

		printf("Validating and deserializing metadata.\n");

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

	///	<summary>
	///		Check if the repo contains the given dataset name.
	///	</summary>
	int find_dataset(const std::string & strDatasetName) const {
		for (int i = 0; i < m_vecDatasetNames.size(); i++) {
			if (m_vecDatasetNames[i] == strDatasetName) {
				return i;
			}
		}
		return (-1);
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
	///		Set metadata from another AutodatamanRepoDatasetMD.
	///	</summary>
	void set_from_admdataset(
		const AutodatamanRepoDatasetMD & admdataset
	) {
		m_strShortName = admdataset.m_strShortName;
		m_strLongName = admdataset.m_strLongName;
		m_strSource = admdataset.m_strSource;
		m_strDefaultVersion = admdataset.m_strDefaultVersion;
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

		// Default version
		auto itjdefault = itjdataset->find("default");
		if (itjdefault == itjdataset->end()) {
			_EXCEPTIONT("Malformed repository metadata file (missing \"_DATASET::default\" key)");
		}
		if (!itjdefault.value().is_string()) {
			_EXCEPTIONT("Malformed repository metadata file (\"_DATASET::default\" must be type \"string\")");
		}
		m_strDefaultVersion = itjdefault.value();

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

		printf("Dataset contains %lu version(s)\n", m_vecDatasetVersions.size());

		return 0;
	}

	///	<summary>
	///		Construct a JSON object from this object.
	///	</summary>
	void to_JSON(
		nlohmann::json & jmeta
	) {
		jmeta["_DATASET"]["short_name"] = m_strShortName;
		jmeta["_DATASET"]["long_name"] = m_strLongName;
		jmeta["_DATASET"]["source"] = m_strSource;
		jmeta["_DATASET"]["default"] = m_strDefaultVersion;
		nlohmann::json & jsonFiles = jmeta["_VERSIONS"] = nlohmann::json::array();
		for (auto i = 0; i < m_vecDatasetVersions.size(); i++) {
			jsonFiles.push_back(m_vecDatasetVersions[i]);
		}
	}

	///	<summary>
	///		Write the metadata file.
	///	</summary>
	void to_file(
		const std::string & strFile
	) {
		std::ofstream ofs(strFile);
		if (!ofs.is_open()) {
			_EXCEPTION1("Unable to open file \"%s\" for writing", strFile.c_str());
		}
		nlohmann::json jmeta;
		to_JSON(jmeta);
		ofs << jmeta;
		ofs.close();
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
	///		Populate from local repository.
	///	</summary>
	int from_local_repo(
		const std::string & strRepoPath,
		const std::string & strDataset
	) {
		// Repo metadata
		std::string strRepoMetadataPath = strRepoPath;
		if (strRepoMetadataPath[strRepoMetadataPath.length()-1] != '/') {
			strRepoMetadataPath += "/";
		}
		strRepoMetadataPath += strDataset + std::string("/dataset.txt");

		std::ifstream ifs(strRepoMetadataPath);
		if (!ifs.is_open()) {
			_EXCEPTION1("Unable to open repository metadata file \"%s\"",
				strRepoMetadataPath.c_str());
		}
		nlohmann::json jmeta;
		try {
			jmeta = nlohmann::json::parse(ifs);
		} catch (nlohmann::json::parse_error& e) {
			_EXCEPTION3("Malformed repository metadata file "
				"%s (%i) at byte position %i",
				e.what(), e.id, e.byte);
		}

		printf("Validating and deserializing metadata.\n");

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
	///		Get the dataset source.
	///	</summary>
	const std::string & get_default_version() const {
		return m_strDefaultVersion;
	}

	///	<summary>
	///		Get the vector of dataset names.
	///	</summary>
	const std::vector<std::string> & get_dataset_versions() const {
		return m_vecDatasetVersions;
	}

	///	<summary>
	///		Add the given version name to the repo.
	///	</summary>
	void add_version(const std::string & strVersion) {
		if (find_version(strVersion) != (-1)) {
			_EXCEPTION1("Trying to add existing version \"%s\"", strVersion.c_str());
		}
		m_vecDatasetVersions.push_back(strVersion);
	}

	///	<summary>
	///		Check if the repo contains the given version name.
	///	</summary>
	int find_version(const std::string & strVersion) const {
		for (int i = 0; i < m_vecDatasetVersions.size(); i++) {
			if (m_vecDatasetVersions[i] == strVersion) {
				return i;
			}
		}
		return (-1);
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

class AutodatamanRepoFileMD {

public:
	///	<summary>
	///		Populate from JSON object.
	///	</summary>
	int from_JSON(
		nlohmann::json & jmeta
	) {
		// Filename
		auto itjfilename = jmeta.find("filename");
		if (itjfilename == jmeta.end()) {
			_EXCEPTIONT("Malformed repository metadata file (missing \"_FILES::filename\" key)");
		}
		if (!itjfilename.value().is_string()) {
			_EXCEPTIONT("Malformed repository metadata file (\"_FILES::filename\" must be type \"string\")");
		}
		m_strFilename = itjfilename.value();

		// SHA256sum
		auto itjsha256sum = jmeta.find("SHA256sum");
		if (itjsha256sum == jmeta.end()) {
			_EXCEPTIONT("Malformed repository metadata file (missing \"_FILES::SHA256sum\" key)");
		}
		if (!itjsha256sum.value().is_string()) {
			_EXCEPTIONT("Malformed repository metadata file (\"_FILES::SHA256sum\" must be type \"string\")");
		}
		m_strSHA256sum = itjsha256sum.value();

		// Format
		auto itjformat = jmeta.find("format");
		if (itjformat == jmeta.end()) {
			_EXCEPTIONT("Malformed repository metadata file (missing \"_FILES::format\" key)");
		}
		if (!itjformat.value().is_string()) {
			_EXCEPTIONT("Malformed repository metadata file (\"_FILES::format\" must be type \"string\")");
		}
		m_strFormat = itjformat.value();

		// OnDownload (optional)
		auto itjondownload = jmeta.find("on_download");
		if (itjondownload != jmeta.end()) {
			if (!itjondownload.value().is_string()) {
				_EXCEPTIONT("Malformed repository metadata file (\"_FILES::format\" must be type \"string\")");
			}

			m_strOnDownload = itjondownload.value();
		}

		return 0;
	}

	///	<summary>
	///		Construct a JSON object from this object.
	///	</summary>
	void to_JSON(
		nlohmann::json & jmeta
	) {
		jmeta["filename"] = m_strFilename;
		jmeta["SHA256sum"] = m_strSHA256sum;
		jmeta["format"] = m_strFormat;
		jmeta["on_download"] = m_strOnDownload;
	}

	///	<summary>
	///		Get the filename.
	///	</summary>
	const std::string & get_filename() const {
		return m_strFilename;
	}

	///	<summary>
	///		Get the SHA256sum.
	///	</summary>
	const std::string & get_sha256sum() const {
		return m_strSHA256sum;
	}

	///	<summary>
	///		Get the format.
	///	</summary>
	const std::string & get_format() const {
		return m_strFormat;
	}

	///	<summary>
	///		Get the format.
	///	</summary>
	const std::string & get_ondownload() const {
		return m_strOnDownload;
	}

	///	<summary>
	///		Print summary to screen.
	///	</summary>
	void summary() const {
		printf("  Filename:   %s\n", m_strFilename.c_str());
		printf("  SHA256sum:  %s\n", m_strSHA256sum.c_str());
		printf("  Format:     %s\n", m_strFormat.c_str());
		printf("  OnDownload: %s\n", m_strOnDownload.c_str());
	}

	///	<summary>
	///		Equivalence operator.
	///	</summary>
	bool operator== (const AutodatamanRepoFileMD & admfilemd) const {
		return (
			(m_strFilename == admfilemd.m_strFilename) &&
			(m_strSHA256sum == admfilemd.m_strSHA256sum) &&
			(m_strFormat == admfilemd.m_strFormat) &&
			(m_strOnDownload == admfilemd.m_strOnDownload));
	}

protected:
	///	<summary>
	///		Filename.
	///	</summary>
	std::string m_strFilename;

	///	<summary>
	///		SHA256sum.
	///	</summary>
	std::string m_strSHA256sum;

	///	<summary>
	///		Format.
	///	</summary>
	std::string m_strFormat;

	///	<summary>
	///		Source.
	///	</summary>
	std::string m_strOnDownload;
};

///////////////////////////////////////////////////////////////////////////////

class AutodatamanRepoDataMD {

public:
	///	<summary>
	///		Clear the repo.
	///	</summary>
	void clear() {
		m_strVersion = "";
		m_strDate = "";
		m_strSource = "";
	}

	///	<summary>
	///		Populate from JSON object.
	///	</summary>
	int from_JSON(
		nlohmann::json & jmeta
	) {
		// Convert to local object
		auto itjdata = jmeta.find("_DATA");
		if (itjdata == jmeta.end()) {
			_EXCEPTIONT("Malformed repository metadata file (missing \"_DATA\" key)");
		}

		// Version
		auto itjversion = itjdata->find("version");
		if (itjversion == itjdata->end()) {
			_EXCEPTIONT("Malformed repository metadata file (missing \"_DATA::version\" key)");
		}
		if (!itjversion.value().is_string()) {
			_EXCEPTIONT("Malformed repository metadata file (\"_DATA::version\" must be type \"string\")");
		}
		m_strVersion = itjversion.value();

		// Date
		auto itjdate = itjdata->find("date");
		if (itjdate == itjdata->end()) {
			_EXCEPTIONT("Malformed repository metadata file (missing \"_DATA::date\" key)");
		}
		if (!itjdate.value().is_string()) {
			_EXCEPTIONT("Malformed repository metadata file (\"_DATA::date\" must be type \"string\")");
		}
		m_strDate = itjdate.value();

		// Source
		auto itjsource = itjdata->find("source");
		if (itjsource == itjdata->end()) {
			_EXCEPTIONT("Malformed repository metadata file (missing \"_DATA::source\" key)");
		}
		if (!itjsource.value().is_string()) {
			_EXCEPTIONT("Malformed repository metadata file (\"_DATA::source\" must be type \"string\")");
		}
		m_strSource = itjsource.value();

		// Files
		auto itjfiles = jmeta.find("_FILES");
		if (itjfiles == jmeta.end()) {
			_EXCEPTIONT("Malformed repository metadata file (missing \"_FILES\" key)");
		}
		if (!itjfiles.value().is_array()) {
			_EXCEPTIONT("Malformed repository metadata file (\"_FILES\" must be type \"array\")");
		}

		for (size_t s = 0; s < itjfiles->size(); s++) {
			nlohmann::json & jfile = (*itjfiles)[s];
			if (!jfile.is_object()) {
				_EXCEPTIONT("Malformed repository metadata file (\"_FILES\" must be an array of objects");
			}
			AutodatamanRepoFileMD admfilemd;
			int iStatus = admfilemd.from_JSON(jfile);
			_ASSERT(iStatus == 0);
			m_vecFiles.push_back(admfilemd);
		}

		printf("Dataset contains %lu files(s)\n", m_vecFiles.size());

		return 0;
	}

	///	<summary>
	///		Construct a JSON object from this object.
	///	</summary>
	void to_JSON(
		nlohmann::json & jmeta
	) {
		jmeta["_DATA"]["version"] = m_strVersion;
		jmeta["_DATA"]["date"] = m_strDate;
		jmeta["_DATA"]["source"] = m_strSource;
		nlohmann::json & jsonFiles = jmeta["_FILES"] = nlohmann::json::array();
		for (auto i = 0; i < m_vecFiles.size(); i++) {
			nlohmann::json jfilemeta;
			m_vecFiles[i].to_JSON(jfilemeta);
			jsonFiles.push_back(jfilemeta);
		}
	}

	///	<summary>
	///		Write the metadata file.
	///	</summary>
	void to_file(
		const std::string & strFile
	) {
		std::ofstream ofs(strFile);
		if (!ofs.is_open()) {
			_EXCEPTION1("Unable to open file \"%s\" for writing", strFile.c_str());
		}
		nlohmann::json jmeta;
		to_JSON(jmeta);
		ofs << jmeta;
		ofs.close();
	}

	///	<summary>
	///		Populate from server.
	///	</summary>
	int from_server(
		const std::string & strServer,
		const std::string & strDataset,
		const std::string & strVersion
	) {
		// Repo metadata
		std::string strServerMetadata = strServer;
		if (strServer[strServer.length()-1] != '/') {
			strServerMetadata += "/";
		}
		strServerMetadata += strDataset + std::string("/");
		strServerMetadata += strVersion + std::string("/data.txt");
 
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

		printf("Validating and deserializing metadata.\n");

		return from_JSON(jmeta);
	}

	///	<summary>
	///		Populate from local repository.
	///	</summary>
	int from_local_repo(
		const std::string & strRepoPath,
		const std::string & strDataset,
		const std::string & strVersion
	) {
		// Repo metadata
		std::string strRepoMetadataPath = strRepoPath;
		if (strRepoMetadataPath[strRepoMetadataPath.length()-1] != '/') {
			strRepoMetadataPath += "/";
		}
		strRepoMetadataPath += strDataset + std::string("/");
		strRepoMetadataPath += strVersion + std::string("/data.txt");

		std::ifstream ifs(strRepoMetadataPath);
		if (!ifs.is_open()) {
			_EXCEPTION1("Unable to open repository metadata file \"%s\"",
				strRepoMetadataPath.c_str());
		}
		nlohmann::json jmeta;
		try {
			jmeta = nlohmann::json::parse(ifs);
		} catch (nlohmann::json::parse_error& e) {
			_EXCEPTION3("Malformed repository metadata file "
				"%s (%i) at byte position %i",
				e.what(), e.id, e.byte);
		}

		printf("Validating and deserializing metadata.\n");

		return from_JSON(jmeta);
	}

	///	<summary>
	///		Print summary to screen.
	///	</summary>
	void summary() const {
		printf("Version:  %s\n", m_strVersion.c_str());
		printf("Date:     %s\n", m_strDate.c_str());
		printf("Source:   %s\n", m_strSource.c_str());

		for (int i = 0; i < m_vecFiles.size(); i++) {
			printf("-- File %i ------\n", i);
			m_vecFiles[i].summary();
		}
	}

	///	<summary>
	///		Equivalence operator.
	///	</summary>
	bool operator== (const AutodatamanRepoDataMD & admdatamd) const {
		if (m_vecFiles.size() != admdatamd.m_vecFiles.size()) {
			return false;
		}
		for (int i = 0; i < m_vecFiles.size(); i++) {
			if (!(m_vecFiles[i] == admdatamd.m_vecFiles[i])) {
				return false;
			}
		}
		return (
			(m_strVersion == admdatamd.m_strVersion) &&
			(m_strDate == admdatamd.m_strDate) &&
			(m_strSource == admdatamd.m_strSource));
	}

	///	<summary>
	///		Get the number of files.
	///	</summary>
	size_t size() const {
		return m_vecFiles.size();
	}

	///	<summary>
	///		Indexing operator.
	///	</summary>
	const AutodatamanRepoFileMD & operator[](size_t s) {
		return m_vecFiles[s];
	}

protected:
	///	<summary>
	///		Version.
	///	</summary>
	std::string m_strVersion;

	///	<summary>
	///		Date.
	///	</summary>
	std::string m_strDate;

	///	<summary>
	///		Source.
	///	</summary>
	std::string m_strSource;

	///	<summary>
	///		Array of file metadata structures.
	///	</summary>
	std::vector<AutodatamanRepoFileMD> m_vecFiles;
};

///////////////////////////////////////////////////////////////////////////////

#endif // _ADMMETADATA_H_

