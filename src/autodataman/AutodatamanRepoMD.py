from pathlib import Path
import json
import requests

def download_from_server(repo_obj,strServerJSON,strServerTxt):
    """Function for accessing repo metadata files on a server. Meant to
    be called within one of the classes defined here.
    Arguments:
        repo_obj: use self
        strServerJSON: The JSON version of the metadata file name
        strServerTxt: The .txt version of the metadata file name.
                      Will be searched if the JSON version not found. 
    """
    try:
        r = requests.get(strServerJSON,allow_redirects=False)
        if r.ok:
            repo_obj.from_JSON(r.json())
        elif r.status_code == 404:
            # JSON not found, try .txt
            r = requests.get(strServerTxt,allow_redirects=False)
            if r.ok:
                repo_obj.from_JSON(json.loads(r.content.decode("utf-8")))
            else:
                raise Exception("Status code "+str(r.status_code)+" in request for "+strServerTxt)
        else:
            raise Exception("Status code "+str(r.status_code)+" in request for "+strServerJSON)
    except requests.exceptions.RequestException as e:
        print("Problem encountered when requesting server metadata")
        raise Exception(e)

class AutodatamanRepoMD: 
    def __init__(self):
        # Version number
        self.m_strVersion = self.version()
        # List of available datasets
        self.m_vecDatasetNames = []

    def version(self):
        # TODO: version management
        return "1"

    def clear(self):
        self.m_strVersion = version()
        self.m_vecDatasetNames = []

    def from_JSON(self,jmeta):
        """Populate from JSON object."""
        # Convert to local object
        if "_REPO" not in jmeta:
            raise SyntaxError("Malformed repository metadata file (missing \"_REPO\" key)")

        if "type" not in jmeta["_REPO"]:
            raise SyntaxError("Malformed repository metadata file (missing \"_REPO::type\" key)")
        if jmeta["_REPO"]["type"] != "autodataman":
            raise SyntaxError("Malformed repository metadata file (invalid \"_REPO::type\" value)")

        if "version" not in jmeta["_REPO"]:
            raise SyntaxError("Malformed repository metadata file (missing \"_REPO::version\" key)")
        self.m_strVersion = jmeta["_REPO"]["version"]      

        # Datasets
        if "_DATASETS" not in jmeta:
            raise SyntaxError("Malformed repository metadata file (missing \"_Datasets\" key)")        
        if not isinstance(jmeta["_DATASETS"],list):
            raise SyntaxError("Malformed repository metadata file (\"_DATASETS\" must be type \"array\")")
        if not all(isinstance(x, str) for x in jmeta["_DATASETS"]):
            raise SyntaxError("Malformed repository metadata file (\"_DATASETS\" must be an array of strings")
        self.m_vecDatasetNames = jmeta["_DATASETS"]

    def to_JSON(self):
        """Construct a JSON object from this object."""
        jmeta = {"_REPO": {}, "_DATASETS": []}
        jmeta["_REPO"]["type"] = "autodataman"
        jmeta["_REPO"]["version"] = self.version()
        jmeta["_DATASETS"] = self.m_vecDatasetNames
        return jmeta       
    
    def to_file(self, strFile):
        """Write the metadata file."""
        jmeta = self.to_JSON()
        with open(strFile,"w") as ofs:
            json.dump(jmeta, ofs, indent=4)

    def from_server(self,strServer,fVerbose=False):
        """Populate from server."""

        # Repo metadata
        strServerMetadataJSON = strServer + "/repo.json"
        strServerMetadataTxt = strServer + "/repo.txt"

        # Notify user
        if fVerbose:
            print("Contacting server {0}".format(strServer))
        
        # Download the metadata file from the server
        download_from_server(self,strServerMetadataJSON,strServerMetadataTxt)

        if fVerbose:
            print("Server contains {0} dataset(s)".format(self.num_datasets()))

    def from_local_repo(self,strRepoPath,fVerbose=False):
        """Populate from local repository."""
        # Repo metadata
        strRepoMetadataPath = strRepoPath + "/repo.json"

        if Path(strRepoMetadataPath).exists():
            with open(strRepoMetadataPath,"r") as ifs:
                jmeta = json.load(ifs)
        else:
            raise FileNotFoundError(strRepoMetadataPath + " not found.")
        
        self.from_JSON(jmeta)

        if fVerbose:
            print("Local repository contains {0} dataset(s).".format(self.num_datasets()))

    def get_version(self):
        """Get the current version number."""
        return self.m_strVersion

    def get_dataset_names(self):
        """Get the vector of dataset names.""" 
        return self.m_vecDatasetNames

    def find_dataset(self, strDatasetName):
        """Check if the repo contains the given dataset name."""
        if strDatasetName in self.m_vecDatasetNames:
            return self.m_vecDatasetNames.index(strDatasetName)
        else:
            return -1
    
    def add_dataset(self, strDataset):
        """Add the given dataset to the repo."""
        self.m_vecDatasetNames.append(strDataset)
    
    def remove_dataset(self,strDataset):
        """Remove the given dataset to the repo."""
        if strDataset in self.m_vecDatasetNames:
            self.m_vecDatasetNames.remove(strDataset)

    def num_datasets(self):
        """Number of datasets in the repo."""
        return len(self.m_vecDatasetNames)

class AutodatamanRepoDatasetMD:
    def __init__(self):
        self.clear()

    def clear(self):
        """Clear the repo."""
        self.m_strShortName = ""
        self.m_strLongName = ""
        self.m_strSource = ""
        self.m_strDefaultVersion = ""
        # List of available datasets
        self.m_vecDatasetVersions = [] 

    def set_from_admdataset(self,admdataset):
        """Set metadata from another AutodatamanRepoDatasetMD."""
        self.m_strShortName = admdataset.m_strShortName
        self.m_strLongName = admdataset.m_strLongName
        self.m_strSource = admdataset.m_strSource
        self.m_strDefaultVersion = admdataset.m_strDefaultVersion
        self.m_vecDatasetVersions = []

    def from_JSON(self,jmeta):
        """Populate from JSON object."""
        self.clear()

        # Convert to local object
        if "_DATASET" not in jmeta:
            raise SyntaxError("Malformed repository metadata file (missing \"_DATASET\" key)")

        if "short_name" not in jmeta["_DATASET"]:
            raise SyntaxError("Malformed repository metadata file (missing \"_DATASET::short_name\" key)")
        if not isinstance(jmeta["_DATASET"]["short_name"],str):
            raise SyntaxError("Malformed repository metadata value (\"_DATASET::short_name\" must be type \"string\")")
        self.m_strShortName = jmeta["_DATASET"]["short_name"]

        if "long_name" not in jmeta["_DATASET"]:
            raise SyntaxError("Malformed repository metadata file (missing \"_DATASET::long_name\" key)")
        if not isinstance(jmeta["_DATASET"]["long_name"],str):
            raise SyntaxError("Malformed repository metadata value (\"_DATASET::long_name\" must be type \"string\")")
        self.m_strLongName = jmeta["_DATASET"]["long_name"]

        if "default" not in jmeta["_DATASET"]:
            raise SyntaxError("Malformed repository metadata file (missing \"_DATASET::default\" key)")
        if not isinstance(jmeta["_DATASET"]["default"],str):
            raise SyntaxError("Malformed repository metadata file (\"_DATASET::default\" must be type \"string\")")
        self.m_strDefaultVersion = jmeta["_DATASET"]["default"]

        if "_VERSIONS" not in jmeta:
            raise SyntaxError("Malformed repository metadata file (missing \"_VERSIONS\" key)")
        if not isinstance(jmeta["_VERSIONS"],list):
            raise SyntaxError("Malformed repository metadata value (\"_VERSIONS\" must be type \"array\")")
        if not all(isinstance(x, str) for x in jmeta["_VERSIONS"]):
            raise SyntaxError("Malformed repository metadata file (\"_VERSIONS\" must be an array of strings")
        self.m_vecDatasetVersions = jmeta["_VERSIONS"]

    def to_JSON(self):
        """Construct a JSON object from this object."""
        jmeta = {"_DATASET": {}, "_VERSIONS": []}
        jmeta["_DATASET"]["short_name"] = self.m_strShortName
        jmeta["_DATASET"]["long_name"] = self.m_strLongName
        jmeta["_DATASET"]["source"] = self.m_strSource
        jmeta["_DATASET"]["default"] = self.m_strDefaultVersion
        jmeta["_VERSIONS"] = self.m_vecDatasetVersions
        return jmeta      

    def to_file(self,strFile):
        """Write the metadata file."""
        jmeta = self.to_JSON()
        with open(str(strFile),"w") as ofs:
            json.dump(jmeta,ofs,indent=4)

    def from_server(self,strServer,strDataset,fVerbose):
        """Populate from server."""
        strServerMetadataJSON = strServer + "/" + strDataset + "/dataset.json"
        strServerMetadataTxt = strServer + "/" + strDataset + "/dataset.txt"

        # Download the metadata file from the server
        download_from_server(self,strServerMetadataJSON,strServerMetadataTxt)

    def from_local_repo(self,strRepoPath,strDataset,fVerbose):
        """Populate from local repository."""
        # Repo metadata
        strRepoMetadataPath = strRepoPath + "/" + strDataset + "/dataset.json"

        if Path(strRepoMetadataPath).exists():
            with open(strRepoMetadataPath,"r") as ifs:
                jmeta = json.load(ifs)
        else:
            raise FileNotFoundError(strRepoMetadataPath + " not found.")
        
        self.from_JSON(jmeta)

        if fVerbose:
            print("Local dataset contains {0} versions(s).".format(self.num_versions()))

    def summary(self):
        """Print summary of dataset."""
        print("Short name:        {0}".format(self.m_strShortName))
        print("Long name:         {0}".format(self.m_strLongName))
        print("Source:            {0}".format(self.m_strSource))
        print("Default version:   {0}".format(self.m_strDefaultVersion))
        print("Available versions: "+", ".join(self.m_vecDatasetVersions))

    def get_short_name(self):
        """Get the dataset short name."""
        return self.m_strShortName

    def get_long_name(self):
        """Get the dataset short name."""
        return self.m_strLongName

    def get_source(self):
        """Get the dataset long name."""
        return self.m_strSource

    def get_default_version(self):
        """Get the dataset source."""
        return self.m_strDefaultVersion

    def get_version_names(self):
        """Get the dataset versions."""
        return self.m_vecDatasetVersions

    def add_version(self, strVersion):
        """Add the given version name to the repo."""
        self.m_vecDatasetVersions.append(str(strVersion))

    def remove_version(self,strVersion):
        """Remove the given version from the repo."""
        if strVersion in self.m_vecDatasetVersions:
            self.m_vecDatasetVersions.remove(str(strVersion))
    
    def find_version(self,strVersion):
        """Check if the repo contains the given version name."""
        if strVersion in self.m_vecDatasetVersions:
            return self.m_vecDatasetVersions.index(strVersion)
        return -1

    def num_versions(self):
        """Number of versions in the dataset."""
        return len(self.m_vecDatasetVersions)

class AutodatamanRepoFileMD:
    def __init__(self):
        self.clear()
    
    def clear(self):
        self.m_strFilename = ""
        self.m_strSHA256sum = ""
        self.m_strFormat = ""
        self.m_strOnDownload = ""
    
    def from_JSON(self, jmeta):
        """Populate from JSON object."""
        self.clear()

        # Filename
        if "filename" not in jmeta:
            raise SyntaxError("Malformed repository metadata file (missing \"_FILES::filename\" key)")
        if not isinstance(jmeta["filename"],str):
            raise SyntaxError("Malformed repository metadata file (\"_FILES::filename\" must be type \"string\")")
        self.m_strFilename = jmeta["filename"]

        # SHA256sum
        if "SHA256sum" not in jmeta:
            raise SyntaxError("Malformed repository metadata file (missing \"_FILES::SHA256sum\" key)")
        if not isinstance(jmeta["SHA256sum"],str):
            raise SyntaxError("Malformed repository metadata file (\"_FILES::SHA256sum\" must be type \"string\")")
        self.m_strSHA256sum = jmeta["SHA256sum"]

        # Format
        if "format" not in jmeta:
            raise SyntaxError("Malformed repository metadata file (missing \"_FILES::format\" key)")
        if not isinstance(jmeta["format"],str):
            raise SyntaxError("Malformed repository metadata file (\"_FILES::format\" must be type \"string\")")
        self.m_strFormat = jmeta["format"]

        # OnDownload (optional)
        if "on_download" in jmeta:
            if not isinstance(jmeta["on_download"],str):
                raise SyntaxError("Malformed repository metadata file (\"_FILES::format\" must be type \"string\")")
            self.m_strOnDownload = jmeta["on_download"]        

    def to_JSON(self):
        """Construct a JSON object from this object."""
        jmeta = {}
        jmeta["filename"] = self.m_strFilename
        jmeta["SHA256sum"] = self.m_strSHA256sum
        jmeta["format"] = self.m_strFormat
        jmeta["on_download"] = self.m_strOnDownload
        return jmeta

    def get_filename(self):
        """Get the filename."""
        return self.m_strFilename

    def get_sha256sum(self):
        """Get the SHA256sum."""
        return self.m_strSHA256sum
    
    def get_format(self):
        """Get the format."""
        return self.m_strFormat

    def get_ondownload(self):
        """Get on download."""
        return self.m_strOnDownload
    
    def summary(self):
        """Print summary to screen."""
        print("  Filename:   {0}".format(self.m_strFilename))
        print("  SHA256sum:  {0}".format(self.m_strSHA256sum))
        print("  Format:     {0}".format(self.m_strFormat))
        print("  OnDownload: {0}".format(self.m_strOnDownload))

    def __eq__(self,admfilemd):
        """Equivalence operator."""
        result = (
            self.m_strFilename == admfilemd.m_strFilename and \
            self.m_strSHA256sum == admfilemd.m_strSHA256sum and \
            self.m_strFormat == admfilemd.m_strFormat and \
            self.m_strOnDownload == admfilemd.m_strOnDownload)
        return result


class AutodatamanRepoDataMD:
    def __init__(self):
        self.clear()
        self.m_vecFiles = []

    def clear(self):
        """Clear the repo."""
        self.m_strVersion = ""
        self.m_strDate = ""
        self.m_strSource = ""

    def from_JSON(self,jmeta):
        """Populate from JSON object."""

        self.clear()

        if "_DATA" not in jmeta:
            raise SyntaxError("Malformed repository metadata file (missing \"_DATA\" key)")
        
        if "version" not in jmeta["_DATA"]:
            raise SyntaxError("Malformed repository metadata file (missing \"_DATA::version\" key)")
        if not isinstance(jmeta["_DATA"]["version"],str):
            raise SyntaxError("Malformed repository metadata file (\"_DATA::version\" must be type \"string\")")
        self.m_strVersion = jmeta["_DATA"]["version"]

        if "date" not in jmeta["_DATA"]:
            raise SyntaxError("Malformed repository metadata file (missing \"_DATA::date\" key)")
        if not isinstance(jmeta["_DATA"]["date"],str):
            raise SyntaxError("Malformed repository metadata file (\"_DATA::date\" must be type \"string\")")
        self.m_strDate = jmeta["_DATA"]["date"]

        if "source" not in jmeta["_DATA"]:
            raise SyntaxError("Malformed repository metadata file (missing \"_DATA::source\" key)")
        if not isinstance(jmeta["_DATA"]["source"],str):
            raise SyntaxError("Malformed repository metadata file (\"_DATA::source\" must be type \"string\")")
        self.m_strSource = jmeta["_DATA"]["source"]
        
        if not "_FILES" in jmeta:
            raise SyntaxError("Malformed repository metadata file (missing \"_FILES\" key)")
        if not isinstance(jmeta["_FILES"],list):
            raise SyntaxError("Malformed repository metadata file (\"_FILES\" must be type \"array\")")
        for jfile in jmeta["_FILES"]:
            if not isinstance(jfile,dict):
                raise SyntaxError("Malformed repository metadata file (\"_FILES\" must be an array of objects)")
            admfilemd = AutodatamanRepoFileMD()
            admfilemd.from_JSON(jfile)
            self.m_vecFiles.append(admfilemd)

    def to_JSON(self):
        """Construct a JSON object from this object."""
        jmeta = {"_DATA": {}, "_FILES": []}
        jmeta["_DATA"]["version"] = self.m_strVersion
        jmeta["_DATA"]["date"] = self.m_strDate
        jmeta["_DATA"]["source"] = self.m_strSource
        for item in self.m_vecFiles:
            jmeta["_FILES"].append(item.to_JSON())
        return jmeta

    def to_file(self,strFile):
        """Write the metadata file."""
        jmeta = self.to_JSON()
        with open(str(strFile),"w") as ofs:
            json.dump(jmeta,ofs,indent=4)
    
    def from_server(self,strServer,strDataset,strVersion,fVerbose):
        """Populate from server."""
        strServerMetadataJSON = strServer + "/" + strDataset + "/" + strVersion + "/data.json"
        strServerMetadataTxt = strServer + "/" + strDataset + "/" + strVersion + "/data.txt"

        if (fVerbose):
            print("== SERVER DATASET/VERSION METADATA FILE =====")
            print(strServerMetadataJSON)
            print("=============================================")
        
        download_from_server(self,strServerMetadataJSON,strServerMetadataTxt)

        if fVerbose:
            print("Local version contains {0} file(s).\n".format(self.num_files()))
    
    def from_local_repo(self,strRepoPath,strDataset,strVersion,fVerbose):
        """Populate from local repository.""" 
        strRepoMetadataPath = strRepoPath + "/" + strDataset + "/" + strVersion + "/data.json"

        if Path(strRepoMetadataPath).exists():
            with open(str(strRepoMetadataPath),"r") as ifs:
                jmeta = json.load(ifs)
        else:
            raise FileNotFoundError(strRepoMetadataPath + " not found.")

        self.from_JSON(jmeta)

        if (fVerbose): print("Local version contains {0} file(s).", self.num_files())
    
    def summary(self):
        """Print summary to screen."""
        print("Version:  {0}".format(self.m_strVersion))
        print("Date:     {0}".format(self.m_strDate))
        print("Source:   {0}".format(self.m_strSource))
        for item in self.m_vecFiles:
            print("-- File {0}------".format(item))
            item.summary()

    def __eq__(self,admdatamd):
        """Equivalence operator."""
        if len(self.m_vecFiles) != len(admdatamd.m_vecFiles):
            return False
        result = (
            self.m_vecFiles == admdatamd.m_vecFiles and \
            self.m_strVersion == admdatamd.m_strVersion and \
            self.m_strDate == admdatamd.m_strDate and \
            self.m_strSource == admdatamd.m_strSource)
        return result
    
    def num_files(self):
        """Get the number of files."""
        return len(self.m_vecFiles)

    def __getitem__(self,s):
        """Indexing operator."""
        return self.m_vecFiles[s]
