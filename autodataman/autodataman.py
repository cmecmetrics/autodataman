#!/bin/env/python
#
# main script for autodataman
# Based on original code by Paul Ullrich
# 

from autodataman.AutodatamanRepoMD import AutodatamanRepoMD, AutodatamanRepoDatasetMD, AutodatamanRepoFileMD, AutodatamanRepoDataMD
import autodataman.Namelist as Namelist
import json
import hashlib
from pathlib import Path
import subprocess
import shutil
import argparse
import sys
import os
import requests

class AutodatamanNamelist(Namelist.Namelist):
    def __init__(self): 
        super().__init__()
        self.m_path = ""

    def isValidVariable(self,strVar): 
        if strVar in ["tgz_open_command", "default_local_repo", "default_server"]:
            return True
        return False

    def SetDefault(self): 
        self["tgz_open_command"] = "tar -xzf"

    def InitializePath(self):
        homedir = Path.home()

        if homedir.exists():
            self.m_path = homedir/Path(".autodataman")
        else:
            raise NotADirectoryError("Invalid home directory path {0}".format(str(homedir)))

    def LoadFromUser(self):
        self.InitializePath()
        if self.m_path.exists():
            # load nml from file
            self.FromFile(str(self.m_path))
        else:
            self.SetDefault()
            self.SaveToUser()

    def SaveToUser(self):
        return self.ToFile(str(self.m_path))

def get_dataset_name_version(strDataset):
    if '/' in strDataset:
        tmp = strDataset.split('/')
        if len(tmp) > 2:
            raise SyntaxError("Multiple forward-slash characters in dataset specifier\n")
        if len(tmp) == 2:
            strDatasetName,strDatasetVersion = tmp[0],tmp[1]
    else:
        strDatasetName = strDataset
        strDatasetVersion = ""
    return strDatasetName, strDatasetVersion

def adm_getrepo_string(validate = False): 
    nml = AutodatamanNamelist()
    nml.LoadFromUser()
    str_return = nml["default_local_repo"]
    if validate:
        if str_return is None:
            raise Exception("The 'default_local_repo' is not set.\nUse 'autodataman setrepo <local_repo> to set.")
    return str_return

def adm_getserver_string(validate=False): 
    nml = AutodatamanNamelist()
    nml.LoadFromUser()
    str_return = nml["default_server"]
    if validate:
        if str_return is None:
            raise Exception("The 'default_server' is not set.\nUse 'autodataman setserver <server_name> to set.")
    return str_return

def adm_config_get():
    # Load .autodataman
    nml = AutodatamanNamelist()
    nml.LoadFromUser()
    print("Configuration:\n")
    for item in nml.Settings():
        print(" {0} = {1}\n".format(item, nml[item]))

def adm_config_set(strVariable,strValue):
    nml = AutodatamanNamelist()
    nml.LoadFromUser()
    nml[strVariable] = strValue
    nml.SaveToUser()

def adm_initrepo(strDir):
    pathRepo = Path(strDir)
    if pathRepo.exists():
        raise OSError("Unable to create directory {0}: Specified path already exists".format(strDir))
    try:
        pathRepo.mkdir()
    except:
        print("Unable to create directory {0}: Failed in call to mkdir".format(strDir))

    try:
        # Create a new JSON file in the directory
        admmeta = AutodatamanRepoMD()
        pathMeta = pathRepo/"repo.json"
        admmeta.to_file(str(pathMeta))
    except:
        print("Could not create repo.json file")
        print("Repository creation unsuccessful")
        shutil.rmtree(strDir)
        return
    print("New autodataman repo \"{0}\" created successfully".format(strDir))

def adm_setrepo(strDir):
    pathRepo = Path(strDir)

    # Check for existence of repo path
    if not pathRepo.exists():
        raise FileNotFoundError("Repo path {0} not found".format(strDir))
    
    # Verify directory is a valid repository
    pathRepoMeta = pathRepo/"repo.json"
    if not pathRepoMeta.exists():
        raise Exception("The directory {0} is not a valid autodataman repo: Missing \"repo.json\" file".format(pathRepo))
    
    # Set this repository to be default in .autodataman
    nml = AutodatamanNamelist()
    nml.LoadFromUser()
    nml["default_local_repo"] = strDir
    nml.SaveToUser()
    print("Default autodataman repo set to {0}".format(strDir))

def adm_getrepo():
    strRepo = adm_getrepo_string()
    print("Default autodataman repo set to {0}".format(strRepo))

def adm_setserver(strServer):
    # Downlad the server listing
    print("Connecting to server {0}".format(strServer))
    admrepo = AutodatamanRepoMD()
    try:
        admrepo.from_server(strServer)
    except:
        raise Exception("Could not validate server {0}. ".format(strServer) + \
            "To force, use 'autodataman config --variable default_server --value {0}'".format(strServer))
    print("Remote server contains {0} datasets".format(admrepo.num_datasets()))

    # Set this server to be default in .autodataman
    nml = AutodatamanNamelist()
    nml.LoadFromUser()
    nml["default_server"] = strServer
    nml.SaveToUser()

def adm_getserver():
    # Report the name of the repository in .autodataman
    strServer = adm_getserver_string()
    if strServer is None:
        print("No default autodataman server")
    else:
        print("Default autodataman server set to {0}".format(strServer))

def adm_avail(strServer, fVerbose):
    # Load repository descriptor from remote data server
    admrepo = AutodatamanRepoMD()
    admrepo.from_server(strServer, fVerbose)

    # List available datasets
    if admrepo.num_datasets() == 0:
        print("Server {0} containts no datasets".format(strServer))
    if not fVerbose:
        print("Server {0} contains {1} datasets".format(strServer, admrepo.num_datasets()))
    vecDatasets = admrepo.get_dataset_names()
    for item in vecDatasets:
        print("    {0}".format(item))

def adm_list(strLocalRepo,fVerbose):
    # Load repository descriptor from local repo
    admrepo = AutodatamanRepoMD()
    admrepo.from_local_repo(strLocalRepo)

    # List available datasets
    if admrepo.num_datasets() == 0:
        print("Local repo {0} contains no datasets".format(strLocalRepo))
        return
    print("Local repo {0} contains {1} dataset(s)".format(strLocalRepo,admrepo.num_datasets()))

    vecDatasets = admrepo.get_dataset_names()
    for dataset in vecDatasets: 
        # Load dataset metadata from local data server
        admrepodataset = AutodatamanRepoDatasetMD()
        admrepodataset.from_local_repo(strLocalRepo, dataset, fVerbose)

        # Display versions
        vecVersions = admrepodataset.get_version_names()
        for version in vecVersions:
            print(    "{0}/{1}".format(dataset, version))
        if len(vecVersions) == 0:
            print("    {0} (0 versions)".format(dataset))

def adm_info(strServer, strLocalRepo, strDataset, fVerbose):
    print("\n== SERVER COPY ==============================")
    print("Server {0}".format(strServer))

    try:
        # Load repository metadata from remote data server
        admserver = AutodatamanRepoMD()
        admserver.from_server(strServer, fVerbose)

        # Verify requested dataset exists in the repository
        iServerDataset = admserver.find_dataset(strDataset)
        if iServerDataset is None:
            print("Dataset {0} not found on remote server".format(strDataset))
        else:
            # Load dataset metadata from remote data server
            admrepodataset = AutodatamanRepoDatasetMD()
            admrepodataset.from_server(strServer, strDataset, fVerbose)

            # Output information
            print("Long name:  {0}".format(admrepodataset.get_long_name()))
            print("Short name: {0}".format(admrepodataset.get_short_name()))
            if len(admrepodataset.get_source()) != 0:
                print("Source:      {0}".format(admrepodataset.get_source()))
            else:
                print("Source:      (unknown)")
            print("Version(s) available:")

            # List available datasets
            vecVersions = admrepodataset.get_version_names()
            for item in vecVersions:
                if item == admrepodataset.get_default_version():
                    print("  {0} [default]".format(item))
                else:
                    print("   {0}".format(item))
    except Exception as e:
        print("\nThe following error occurred while attempting\nto access server repository information:")
        print(e)

    print("\n== LOCAL COPY ===============================")
    print("Local repo {0}".format(strLocalRepo))

    try:
        # Load repository metadata from local data server
        admlocal = AutodatamanRepoMD()
        admlocal.from_local_repo(strLocalRepo, fVerbose)

        iLocalDataset = admlocal.find_dataset(strDataset)
        if iLocalDataset is None:
            print("Dataset {0} not found in local repo".format(strDataset))
        else:
            # Load dataset metadata from local data server
            admrepodataset = AutodatamanRepoDatasetMD()
            admrepodataset.from_local_repo(strLocalRepo, strDataset, fVerbose)

            # Output information
            print("Long name: {0}".format(admrepodataset.get_long_name()))
            print("Short name: {0}".format(admrepodataset.get_short_name()))
            if len(admrepodataset.get_source()) != 0:
                print("Source:      {0}".format(admrepodataset.get_source()))
            else:
                print("Source:      (unknown)")
            print("Version(s) available:")

            # List available datasets
            vecVersions = admrepodataset.get_version_names()
            for item in vecVersions:
                if item == admrepodataset.get_default_version():
                    print("  {0} [default]".format(item))
                else:
                    print("  {0}".item)
            if len(vecVersions) == 0:
                print("  (none)")
    except Exception as e:
        print("\nThe following error occurred while attempting\nto access local repository information:")
        print(e)
    print("=============================================")

def adm_remove(strLocalRepo,strDataset,fRemoveAll,fVerbose):
    # Check arguments
    if len(strLocalRepo) == 0:
        raise SyntaxError("Missing local repo path")
    if len(strDataset) == 0:
        raise SyntaxError("Missing dataset name")
    
    # Repository information
    print("Local repo {0}".format(strLocalRepo))

    # Break up dataset into name and version
    strDatasetName, strDatasetVersion = get_dataset_name_version(strDataset)

    # Path to repo
    pathRepo = Path(strLocalRepo)
    if len(strLocalRepo) < 2:
        raise Exception("Invalid local repository name {0}".format(str(pathRepo)))

    # Get the local repo metadata
    admlocalrepo = AutodatamanRepoMD()
    admlocalrepo.from_local_repo(strLocalRepo, fVerbose)

    iLocalDataset = admlocalrepo.find_dataset(strDatasetName)
    if iLocalDataset is None:
        print("Dataset {0} not found in local repo".format(strDatasetName))
    
    # Path to dataset
    pathDataset = pathRepo/Path(strDatasetName)

    if not pathDataset.exists():
        raise FileNotFoundError("Damaged local repo.  Path {0} does not exist in repo, ".format(str(pathDataset)) +
            "but is referenced in repo metadata.  Try running 'repair' on repo.")

    if not pathDataset.is_dir():
        raise NotADirectoryError("Damaged local repo.  Path {0} is not a directory, ".format(str(pathDataset)) +
            "but is referenced in repo metadata.  Try running 'repair' on repo.")

    # Load dataset metadata
    admlocaldataset = AutodatamanRepoDatasetMD()
    admlocaldataset.from_local_repo(strLocalRepo, strDatasetName, fVerbose)

    # No version specified; try to remove entire dataset
    if strDatasetVersion == "":
        if admlocaldataset.num_versions() == 0:
            fRemoveAll = True
            print("Removing dataset {0} (0 versions)".format(strDatasetName))
        if admlocaldataset.num_versions() == 1:
            fRemoveAll = True
            print("Removing dataset {0} (1 version)".format(strDatasetName))
        
        if not fRemoveAll:
            print("Dataset {0} contains multiple versions ({1}).".format(strDatasetName,admlocaldataset.num_versions()))
            print("To remove entire dataset rerun with '-a'.")
            raise RuntimeError
        else:
            admlocalrepo.remove_dataset(strDatasetName)
            try:
                shutil.rmtree(str(pathDataset))
            except:
                raise Exception("Command failed. Could not remove directory {0}".format(strDatasetName))
    else:
        # Check that version exists in metadata
        iLocalVersion = admlocaldataset.find_version(strDatasetVersion)
        if iLocalVersion is None:
            raise KeyError("Version {0} not found in local dataset {1}".format(strDatasetVersion,strDatasetName))

        # Remove from metadata
        admlocaldataset.remove_version(strDatasetVersion)

        # Path to version
        pathVersion = pathDataset/strDatasetVersion
        if not pathVersion.exists():
            raise RuntimeError("Damaged local repo.  Path {0} does not exist in repo, ".format(str(pathVersion)) + \
                "but is referenced in dataset metadata.  Try running \"repair\" on repo.")
        
        if not pathVersion.is_dir():
            raise NotADirectoryError("Damaged local repo.  Path {0} is not a directory, ".format(pathVersion) + \
                "but is referenced in dataset metadata.  Try running \"repair\" on repo.")
        
        # Execute removal of version directory
        try:
            shutil.rmtree(str(pathVersion))
        except:
            raise Exception("Command failed. Could not remove directory {0}".format(str(pathVersion)))

    # If these commands trigger an exception we've likely corrupted the repo
    try:
        # Write updated repo metadata
        if strDatasetVersion == "":
            pathRepoMetaJSON = pathRepo/"repo.json"
            if fVerbose:
                print("Writing repo metadata to {0} (contains {1} datasets)".format(str(pathRepoMetaJSON),admlocalrepo.num_datasets()))
            admlocalrepo.to_file(str(pathRepoMetaJSON))
        # Write updated dataset metadata
        if strDatasetVersion !=  "":
            pathDatasetMetaJSON = pathDataset/"dataset.json"
            if fVerbose:
                print("Writing dataset metadata to {0} (containts {1} versions)".format(str(pathDatasetMetaJSON),admlocaldataset.num_versions()))
            admlocaldataset.to_file(str(pathDatasetMetaJSON))
    except:
        raise RuntimeError("DANGER: Exception may have corrupted repository.\nRun \"validate\" to check.")
    
    # Success
    print("Dataset {0} removed successfully".format(strDataset))

def adm_get(strServer, strLocalRepo, strDataset, fForceOverwrite, fVerbose):
    # Check arguments
    if len(strServer) == 0:
        raise RuntimeError("Missing server url")
    if len(strLocalRepo) == 0:
        raise RuntimeError("Missing local repo path")
    if len(strDataset) == 0:
        raise RuntimeError("Missing dataset name")
    
    # Output arguments
    print("Local repo {0}".format(strLocalRepo))
    print("Server {0}".format(strServer))

    # Break up dataset into name and version
    strDatasetName, strDatasetVersion = get_dataset_name_version(strDataset)

    # Path to repo
    if len(strLocalRepo) < 2:
        raise Exception("Invalid local repository name {0}".format(strLocalRepo))
    pathRepo = Path(strLocalRepo).absolute()

    # Path to dataset locally
    pathDataset = pathRepo / Path(strDatasetName)

    # Flag indicating that this is a new dataset
    fNewDataset = False

    # Flag indicating that the current version already exists, should be overwritten
    fOverwriteVersion = False

    # Load repository descriptor from remote data server
    admserver = AutodatamanRepoMD()
    admserver.from_server(strServer, fVerbose)

    # Verify requested dataset exists in the repository
    iServerDataset = admserver.find_dataset(strDatasetName)
    if iServerDataset < 0:
        raise Exception("Dataset {0} not found on remote server".format(strDatasetName))

    # Load dataset descriptor from remote data server
    admserverdataset = AutodatamanRepoDatasetMD()
    admserverdataset.from_server(strServer, strDatasetName, fVerbose)

    # Use default version number, if not specified
    if strDatasetVersion == "":
        strDatasetVersion = admserverdataset.get_default_version()
        if strDatasetVersion == "":
            print("No default version of dataset {0} found.".format(strDatasetName))
            print("Please specify a version:")
            vecVersions = admserverdataset.get_version_names()
            for item in vecVersions:
                print("  {0}".format(item))
            return
        print("Default dataset version is {0}".format(strDatasetVersion))

    # Verify requested version exists in the repository
    iServerVersion = admserverdataset.find_version(strDatasetVersion)
    if iServerVersion < 0:
        raise Exception("Dataset {0} version {1} not found on remote server.".format(strDatasetName,strDatasetVersion))
    
    # Download data descriptor from remote data server
    admserverdata = AutodatamanRepoDataMD()
    admserverdata.from_server(strServer, strDatasetName, strDatasetVersion, fVerbose)

    # Load repository descriptor from local data server
    admlocalrepo = AutodatamanRepoMD()
    admlocalrepo.from_local_repo(strLocalRepo,fVerbose)

    # Path to version locally
    pathVersion = pathDataset/strDatasetVersion

    # Path to directory storing downloaded data
    # This is the same as pathVersion unless fOverwriteVersion is true
    pathVersionTemp = pathVersion

    # Check if dataset exists in the local repository
    admlocaldataset = AutodatamanRepoDatasetMD()
    iLocalDataset = admlocalrepo.find_dataset(strDatasetName)

    # Text of exception if code aborts
    strAbortText = ""

    # Catch all exceptions from here so we can clean up data directory
    try:
        # Dataset does not exist in local repository
        if iLocalDataset < 0:
            # Check for existence of dataset directory
            if pathDataset.exists():
                raise Exception("Damaged local repo. Path {0} already exists in repo".format(str(pathDataset)) + 
                " but not referenced in repo metadata. Try running \"repair\" on repo.")
            
            # Create dataset directory
            pathDataset.mkdir()
            # New dataset
            fNewDataset = True

            # Copy metadata
            admlocaldataset.set_from_admdataset(admserverdataset)

            # Add to repo
            admlocalrepo.add_dataset(strDatasetName)
        # Dataset exists in local repository
        else:
            # Check for existence of dataset path
            if not pathDataset.exists():
                raise Exception("Damaged local repo. Path {0} does not exist in repo, ".format(str(pathDataset)) +
                "but is referenced in repo metadata. Try running \"repair\" on repo.")

            # Verify dataset path is a directory
            if not pathDataset.is_dir():
                raise Exception("Damaged local repo.  Path {0} is not a directory, ".format(str(pathDataset)) + 
                    "but is referenced in repo metadata.  Try running \"repair\" on repo.")

            # Load the metadata file
            admlocaldataset.from_local_repo(strLocalRepo,strDatasetName,fVerbose)
        
        #################################################
        # Check if version exists in the local repository
        admlocaldata = AutodatamanRepoDataMD()
        iLocalVersion = admlocaldataset.find_version(strDatasetVersion)
        
        # Version does not exist in dataset
        if iLocalVersion < 0:
            # Check for existence of version directory
            if pathVersionTemp.exists():
                raise Exception("Damaged local repo. Path {0} already exists in repo".format(str(pathVersionTemp)) + 
                " but not referenced in repo metadata. Try running \"repair\" on repo.")
            
            # Add to dataset
            admlocaldataset.add_version(strDatasetVersion)
        
        # Version exists in dataset
        else:
            # Load the local metadata file
            admlocaldata.from_local_repo(strLocalRepo, strDatasetName, strDatasetVersion, fVerbose)
            
            # Check for equivalence
            if admserverdata == admlocaldata:
                print("Specified dataset \"{0}\" already exists on local repo.".format(strDataset))
                if not fForceOverwrite:
                    print("Rerun with \"-f\" to force overwrite.")
                    return
                else:
                    print("Overwriting with server data.")
            else:
                print("== SERVER COPY ==============================")
                admserverdata.summary() 
                print("== LOCAL COPY ===============================")
                admlocaldata.summary()
                print("=============================================")
                print("WARNING: Specified dataset \"{0}\" exists on local repo, ".format(strDataset))
                print("but metadata descriptor does not match.  This could mean ")
                print("that one or both datasets are corrupt.  ")

                if not fForceOverwrite:
                    print("Rerun with \"-f\" to force overwrite.")
                    return
                else:
                    print("Overwriting with server data.")
            
            # Set temporary path for storing data
            pathVersionTemp = pathDataset/(strDatasetVersion+".part")

            # Overwrite
            fOverwriteVersion = True
        
        # Local metadata will be overwritten by server metadata
        admlocaldata = admserverdata

        # Create data directory
        pathVersionTemp.mkdir()

        # Write metadata
        pathDataMetaJSON = pathVersionTemp/"data.json"

        if fVerbose:
            print("Writing version metaata to \"{0}\"".format(str(pathDataMetaJSON)))

        admlocaldata.to_file(str(pathDataMetaJSON))

        ###############
        # Download data
        print("=============================================")
        strRemoteFilePath = strServer
        assert len(strRemoteFilePath) > 0
        strRemoteFilePath = os.path.join(strRemoteFilePath,strDatasetName,strDatasetVersion,"")

        for i in range(0,admlocaldata.num_files()):
            admlocalfile = admlocaldata[i]

            strRemoteFile = strRemoteFilePath + admlocalfile.get_filename()

            pathFile = pathVersionTemp/admlocalfile.get_filename()

            print("Downloading {0}".format(admlocalfile.get_filename()))
            if fVerbose: print("Local target {0}".format(str(pathFile)))

            # TODO: test the chunk size
            r = requests.get(strRemoteFile,allow_redirects=True,stream=True)
            dest_file = open(str(pathFile),"wb")
            for chunk in r.iter_content(chunk_size=128*1024):
                if chunk:
                    dest_file.write(chunk)
            dest_file.close()

            # Check SHA256
            # SHA256 from https://stackoverflow.com/questions/22058048/hashing-a-file-in-python
            h = hashlib.sha256()
            b = bytearray(128*1024)
            mv = memoryview(b)
            with open(pathFile,"rb",buffering=0) as open_file:
                for n in iter(lambda : open_file.readinto(mv),0):
                    h.update(mv[:n])
            strSHA256 = h.hexdigest()

            if strSHA256 != admlocalfile.get_sha256sum():
                print("Repository SHA256 {0}".format(admlocalfile.get_sha256sum()))
                raise Exception("Failed to verify file SHA256. If local data " +
                "repository is inconsistent try \"remove {0}/{1}\" before ".format(strDatasetName,strDatasetVersion) +
                "downloading again.")
            else:
                print("Verified SHA256 {0}".format(strSHA256))

        #############################
        # Apply OnDownload operations

        # Load the namelist
        nml = AutodatamanNamelist()
        nml.LoadFromUser()

        # Apply system commands
        fHasOnDownload = False
        for i in range(0,admlocaldata.num_files()):
            admlocalfile = admlocaldata[i]
            
            pathFile = pathVersionTemp/admlocalfile.get_filename()

            if admlocalfile.get_ondownload() != "" and admlocalfile.get_ondownload() is not None:
                fHasOnDownload = True

                strNamelistVar = admlocalfile.get_format() + "_" \
                    + admlocalfile.get_ondownload() + "_command"
                
                if nml[strNamelistVar] is not None:
                    strCommand = "cd " + str(pathVersionTemp) + " && " \
                        + nml[strNamelistVar] + " " + str(pathFile.name) \
                        + "&& rm " + str(pathFile.name)
                
                    if strCommand != "" and strCommand is not None:
                        print("Executing \"{0}\"".format(strCommand))
                        iResult = subprocess.run(strCommand, shell=True) # TODO: check this run command
                        # TODO: wrap with try/except

        if fHasOnDownload:
            print("=============================================")        

    #################################
    # Cleanup if an exception occurs
    except Exception as e:
        if fNewDataset or fOverwriteVersion:
            print("Exception caused code to abort.  Cleaning up.")
        
        # Remove dataset directory created earlier in this function
        if fNewDataset:
            if pathDataset.exists():
                print("Removing {0}".format(str(pathDataset)))
                shutil.rmtree(str(pathDataset))
        
        # Remove version directory created earlier in this function
        elif fOverwriteVersion:
            if pathVersionTemp.exists():
                print("Removing {0}".format(str(pathVersionTemp)))
                shutil.rmtree(str(pathVersionTemp))

        # Now throw the error
        #print("Error: ",e)
        raise Exception(e)
        return

    ########################################################################
    # If these commands trigger an exception we've likely corrupted the repo
    try:
        # Keep old dataset metadata, but rename version directory
        if fOverwriteVersion:
            print("Removing {0}".format(str(pathVersion)))
            shutil.rmtree(str(pathVersion))

            print("Moving {0} to {1}".format(str(pathVersionTemp),str(pathVersion)))
            shutil.move(str(pathVersionTemp),str(pathVersion))
        else:
            # Write updated dataset metadata
            pathDatasetMetaJSON = pathDataset/"dataset.json"
            if fVerbose:
                print("Writing dataset metadata to {0} (contains {1} version(s))".format(
                    str(pathDatasetMetaJSON),str(admlocaldataset.num_versions())))
            admlocaldataset.to_file(str(pathDatasetMetaJSON))
        
        # Write updated repo metadata
        if fNewDataset:
            pathRepoMetaJSON = pathRepo/"repo.json"
            print("Writing repo metadata to {0} (contains {1} dataset(s))".format(
                str(pathDataMetaJSON),str(admlocalrepo.num_datasets())
            ))
            admlocalrepo.to_file(str(pathRepoMetaJSON))
    except Exception as e:
        # TODO: "validate" doesn't exist yet
        print("DANGER: Exception may have corrupted repository.")
        #print("        Run \"validate\" to check.")
        print("Error: ",e)
        return

    # Success
    print("Dataset {0} retrieved successfully".format(strDataset))

def adm_put():
    # TODO: Not implemented yet.
    pass

def main():

    parser = argparse.ArgumentParser(description="Arguments for autodataman")
    
    subparsers = parser.add_subparsers(dest="command")
    
    # Config
    parser_config = subparsers.add_parser("config", help="Usage: autodataman config [<variable> <value>]")
    parser_config.add_argument("--variable",type=str,help="variable")
    parser_config.add_argument("--value",type=str,help="value")

    # Initiate Repo
    parser_initrepo = subparsers.add_parser("initrepo",help="Usage: autodataman initrepo <local repo dir>")
    parser_initrepo.add_argument("local",type=str,help="directory")

    # Set Repo
    parser_setrepo = subparsers.add_parser("setrepo",help="Usage: autodataman initrepo <local repo dir>")
    parser_setrepo.add_argument("local",type=str,help="directory")

    # Set Server
    parser_setserver = subparsers.add_parser("setserver",help="Usage: autodataman setserver <remote server>")
    parser_setserver.add_argument("server",type=str,help="server")

    # Get Server
    parser_getserver = subparsers.add_parser("getserver",help="Usage: autodataman getserver")

    # Avail
    parser_avail = subparsers.add_parser("avail",help="Usage: autodataman avail [-s <server>]")
    parser_avail.add_argument("--server","-s",type=str,help="server")
    parser_avail.add_argument("--verbose","-v",action="store_true")

    # Info
    parser_info = subparsers.add_parser("info",help="Usage: autodataman info [-v] [-l <local repo dir>] [-s <server>] <dataset id>")
    parser_info.add_argument("dataset",help="dataset id",type=str)
    parser_info.add_argument("--server","-s",help="server",default=None)
    parser_info.add_argument("--local","-l",help="local repository",default=None)
    parser_info.add_argument("--verbose","-v",help="increase verbosity",action="store_true")

    # List
    parser_list = subparsers.add_parser("list",help="Usage: autodataman list [-l <local repo dir>]")
    parser_list.add_argument("--local","-l",type=str,help="local repository")
    parser_list.add_argument("--verbose","-v",help="increase verbosity",action="store_true")

    # Remove
    parser_remove = subparsers.add_parser("remove",help="Usage: autodataman remove [-v] [-l <local repo dir>] <dataset id>[/<version>]")
    parser_remove.add_argument("dataset",help="dataset id",type=str)
    parser_remove.add_argument("--local","-l",type=str,help="local repository")
    parser_remove.add_argument("--verbose","-v",help="increase verbosity")
    parser_remove.add_argument("--all","-a",help="remove all",action="store_true")

    # Get
    parser_get = subparsers.add_parser("get",help="Usage: autodataman get [-f] [-v] [-l <local repo dir>] [-s <server>] <dataset id>[/<version>]")
    parser_get.add_argument("dataset",help="dataset id",type=str)
    parser_get.add_argument("--server","-s",type=str,help="server")
    parser_get.add_argument("--local","-l",type=str,help="local repository")
    parser_get.add_argument("--verbose","-v",help="increase verbosity",action="store_true")
    parser_get.add_argument("--force","-f",help="force overwrite",action="store_true")

    # TODO: validate
    # TODO: repair

    args = parser.parse_args()
    strCommand = args.command

    # Configure
    if strCommand == "config":
        if args.variable is not None:
            if args.value is None:
                raise Exception("The --value flag must be set when the --variable flag is set.")
            strVariable = args.variable
            strValue = args.value
            adm_config_set(strVariable,strValue)
        else:
            adm_config_get()

    # Initialize a repository directory with no content 
    if strCommand == "initrepo":
        strDir = args.local
        return adm_initrepo(strDir)

    # Set the default repository
    if strCommand == "setrepo":
        strDir = args.local
        return adm_setrepo(strDir)

    # Get the default repository
    if strCommand == "getrepo":
        return adm_getrepo()

    # Set the default server
    if strCommand == "setserver":
        strServer = args.server
        return adm_setserver(strServer)

    # Get the default server
    if strCommand == "getserver":
        return adm_getserver()

    # Check for data available
    if strCommand == "avail":
        if args.server is not None:
            strRemoteServer = args.server
        else:
            strRemoteServer = adm_getserver_string(validate=True)
        fVerbose = args.verbose
        return adm_avail(strRemoteServer,fVerbose)

    # Check for data information
    if strCommand == "info":
        strDataset = args.dataset
        if args.server is not None:
            strRemoteServer = args.server
        else:
            strRemoteServer = adm_getserver_string(validate=True)
        if args.local is not None:
            strLocalRepo = args.local
        else:
            strLocalRepo = adm_getrepo_string(validate=True)
        fVerbose = args.verbose
        return adm_info(strRemoteServer, strLocalRepo, strDataset, fVerbose)

    # Check for data on the local repository
    if strCommand == "list":
        if args.local is not None:
            strLocalRepo = args.local
        else:
            strLocalRepo = adm_getrepo_string(validate=True)
            print("Using default local repository '" + strLocalRepo + "'.")
        fVerbose = args.verbose
        return adm_list(strLocalRepo, fVerbose)

    # Remove data from the local repository
    if strCommand == "remove":
        strDataset = args.dataset
        if args.local is not None:
            strLocalRepo = args.local
        else:
            strLocalRepo = adm_getrepo_string(validate=True)
        fRemoveAll = args.all
        fVerbose = args.verbose
        return adm_remove(strLocalRepo,strDataset,fRemoveAll,fVerbose)

    # Get data from the remote repository and store in the local repository 
    if strCommand == "get":
        strDataset = args.dataset
        if args.server is not None:
            strRemoteServer = args.server
        else:
            strRemoteServer = adm_getserver_string(validate=True)
        if args.local is not None:
            strLocalRepo = args.local
        else:
            strLocalRepo = adm_getrepo_string(validate=True)
        fForceOverwrite = args.force
        fVerbose = args.verbose

        return adm_get(strRemoteServer, strLocalRepo, strDataset, fForceOverwrite, fVerbose)

    # Put data on the server
    #if strCommand == "put":
    #    pass

if __name__ == "__main__":
    main()
