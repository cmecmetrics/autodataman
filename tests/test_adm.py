import json
import os
import pytest
import requests
import sys
from pathlib import Path
import shutil

from autodataman.AutodatamanRepoMD import AutodatamanRepoMD, AutodatamanRepoDatasetMD, AutodatamanRepoFileMD, AutodatamanRepoDataMD
from autodataman.Namelist import Namelist
from autodataman.autodataman import AutodatamanNamelist, get_dataset_name_version, adm_getrepo_string, adm_getserver_string, adm_config_get, \
adm_config_set, adm_initrepo, adm_setrepo, adm_getrepo, adm_setserver, adm_getserver, adm_avail, adm_list, \
adm_info, adm_remove, adm_get

def return_data_dot_json():
        """Return fake data.json."""
        txt_dict = {
                "_DATA": {
                        "version": "v1",
                        "date": "1/1/11",
                        "source": "Test source"
                },
                "_FILES": [
                                {
                                "filename": "test_file_1.nc",
                                "SHA256sum": "abcdefg123",
                                "format": "netCDF",
                                "on_download": ""
                        }
                ]
        }
        return txt_dict

def return_dataset_dot_json():
        """Return fake dataset.json."""
        txt_dict = {
                "_DATASET": {
                        "short_name": "test_1",
                        "long_name": "test_dataset",
                        "default": "v1"

                },
                "_VERSIONS": ["v1"]
        }
        return txt_dict

def return_repo_dot_json():
        """Return fake repo.json."""
        txt_dict = {
                "_REPO": {
                        "type": "autodataman",
                        "version": "1"
                },
                "_DATASETS": ["test_dataset"]
        }
        return txt_dict

def test_admrepo_from_server(requests_mock):
        """Test the AutodatamanRepoMD class with mocked server."""
        strServer="http://test_admrepo_from_server.com"
        fVerbose = False

        mock_dict = return_repo_dot_json()
        requests_mock.get(strServer+"/repo.json",json=mock_dict)

        admrepo = AutodatamanRepoMD()
        admrepo.from_server(strServer, fVerbose)

        assert admrepo.num_datasets() == len(mock_dict["_DATASETS"])
        assert admrepo.get_dataset_names() == mock_dict["_DATASETS"]
        assert admrepo.get_version() == mock_dict["_REPO"]["version"]

def test_admrepo_from_local():
        """Test the AutodatamanRepoMD class with local repository."""
        strRepoPath = "./test_repo"
        fVerbose = False

        with open(strRepoPath+"/repo.json","r") as ifs:
                jmeta = json.load(ifs)

        admrepo = AutodatamanRepoMD()
        admrepo.from_local_repo(strRepoPath, fVerbose)

        assert admrepo.num_datasets() == len(jmeta["_DATASETS"])
        assert admrepo.get_dataset_names() == jmeta["_DATASETS"]
        assert admrepo.get_version() == jmeta["_REPO"]["version"]

def test_admrepo_add_dataset():
        """Test the AutodatamanRepoMD add_dataset() method."""
        strRepoPath = "./test_repo"
        fVerbose = False

        with open(strRepoPath+"/repo.json","r") as ifs:
                jmeta = json.load(ifs)

        admrepo = AutodatamanRepoMD()
        admrepo.from_local_repo(strRepoPath, fVerbose)

        admrepo.add_dataset("test_dataset_2")
        # This dataset should be found in position 1
        assert admrepo.find_dataset("test_dataset_2") == 1
        # This dataset should not be found
        assert admrepo.find_dataset("test_dataset_3") == -1

def test_admrepo_remove_dataset():
        """Test the AutodatamanRepoMD remove_dataset() method."""
        strRepoPath = "./test_repo"
        fVerbose = False

        with open("test_repo/repo.json","r") as ifs:
                jmeta = json.load(ifs)

        admrepo = AutodatamanRepoMD()
        admrepo.from_local_repo(strRepoPath, fVerbose)

        admrepo.add_dataset("test_dataset_2")
        admrepo.remove_dataset("test_dataset")

        # This should be only dataset present now (position 0)
        assert admrepo.find_dataset("test_dataset_2") == 0

def test_admrepo_get_version():
        """Test the AutodatamanRepoMD get_version() method."""
        strRepoPath = "./test_repo"
        fVerbose = False

        with open("test_repo/repo.json","r") as ifs:
                jmeta = json.load(ifs)

        admrepo = AutodatamanRepoMD()
        admrepo.from_local_repo(strRepoPath, fVerbose)

        # Version should match contents of JSON file
        assert admrepo.get_version() == "1"

def test_admrepodata_from_server(requests_mock):
        """Test the AutodatamanRepoDataMD.from_server() method with mocked server."""
        strServer="http://test_admrepodata_from_server.com"
        strDataset="dataset"
        strVersion="version"
        fVerbose = False

        mock_dict = return_data_dot_json()
        requests_mock.get(strServer+"/"+strDataset+"/"+strVersion+"/data.json",json=mock_dict)

        admrepo = AutodatamanRepoDataMD()
        admrepo.from_server(strServer,strDataset,strVersion,fVerbose)

        assert admrepo.num_files() == len(mock_dict["_FILES"])
        assert admrepo.m_strVersion == mock_dict["_DATA"]["version"]
        assert admrepo.m_strDate == mock_dict["_DATA"]["date"]
        assert admrepo.m_strSource == mock_dict["_DATA"]["source"]

def test_admrepodata_from_local():
        """Test the AutodatamanRepoDataMD.from_local_repo() method with test repository."""
        strRepoPath = "./test_repo"
        strDataset="test_dataset"
        strVersion="v1"
        fVerbose = False

        admrepo = AutodatamanRepoDataMD()
        admrepo.from_local_repo(strRepoPath,strDataset,strVersion,fVerbose)

        with open("test_repo/test_dataset/v1/data.json","r") as ifs:
            jmeta = json.load(ifs)

        assert admrepo.num_files() == len(jmeta["_FILES"])
        assert admrepo.m_strVersion == jmeta["_DATA"]["version"]
        assert admrepo.m_strDate == jmeta["_DATA"]["date"]
        assert admrepo.m_strSource == jmeta["_DATA"]["source"]

def test_admrepodata_to_json(requests_mock):
        """Test the AutodatamanRepoDataMD.from_server() method with mocked server."""
        strServer="http://test_admrepo_from_server.com"
        strDataset="dataset"
        strVersion="version"
        fVerbose = False

        mock_dict = return_data_dot_json()
        requests_mock.get(strServer+"/"+strDataset+"/"+strVersion+"/data.json",json=mock_dict)

        admrepo = AutodatamanRepoDataMD()
        admrepo.from_server(strServer,strDataset,strVersion,fVerbose)
        test_json = admrepo.to_JSON()

        assert test_json == mock_dict

def test_admrepodataset_from_server(requests_mock):
        """Test the AutodatamanRepoDatasetMD.from_server() method with mocked server."""
        strServer="http://test_admrepodataset_from_server.com"
        strDataset="dataset"
        fVerbose = False

        mock_dict = return_dataset_dot_json()
        requests_mock.get(strServer+"/"+strDataset+"/dataset.json",json=mock_dict)
        
        adm_repo_dataset = AutodatamanRepoDatasetMD()
        adm_repo_dataset.from_server(strServer,strDataset,fVerbose)

        # Check variables directly. Short name and Long name are mandatory,
        # source and default are optional.
        assert adm_repo_dataset.m_strShortName == mock_dict["_DATASET"]["short_name"]
        assert adm_repo_dataset.m_strLongName == mock_dict["_DATASET"]["long_name"]
        assert adm_repo_dataset.m_strSource == mock_dict["_DATASET"].get("source","")
        assert adm_repo_dataset.m_strDefaultVersion == mock_dict["_DATASET"].get("default","")

        # Check variable access methods
        assert adm_repo_dataset.get_short_name() == mock_dict["_DATASET"]["short_name"]
        assert adm_repo_dataset.get_source() == mock_dict["_DATASET"].get("source","")
        assert adm_repo_dataset.get_default_version() == mock_dict["_DATASET"].get("default","")

        test_version = mock_dict["_VERSIONS"][0]
        assert adm_repo_dataset.find_version(test_version) == 0
        assert adm_repo_dataset.num_versions() == len(mock_dict["_VERSIONS"])
        assert adm_repo_dataset.get_version_names() == mock_dict["_VERSIONS"]

def test_admrepodataset_from_local():
        """Test the AutodatamanRepoDatasetMD.from_server() method with local test repo."""
        strRepoPath = "./test_repo"
        strDataset = "test_dataset"
        fVerbose = False
        
        adm_repo_dataset = AutodatamanRepoDatasetMD()
        adm_repo_dataset.from_local_repo(strRepoPath,strDataset,fVerbose)

        with open("test_repo/test_dataset/dataset.json","r") as ifs:
            jmeta = json.load(ifs)

        # Check variables directly. Short name and Long name are mandatory,
        # source and default are optional.
        assert adm_repo_dataset.m_strShortName == jmeta["_DATASET"]["short_name"]
        assert adm_repo_dataset.m_strLongName == jmeta["_DATASET"]["long_name"]
        assert adm_repo_dataset.m_strSource == jmeta["_DATASET"].get("source","")
        assert adm_repo_dataset.m_strDefaultVersion == jmeta["_DATASET"].get("default","")

        # Check variable access methods
        assert adm_repo_dataset.get_short_name() == jmeta["_DATASET"]["short_name"]
        assert adm_repo_dataset.get_source() == jmeta["_DATASET"].get("source","")
        assert adm_repo_dataset.get_default_version() == jmeta["_DATASET"].get("default","")

        test_version = jmeta["_VERSIONS"][0]
        assert adm_repo_dataset.find_version(test_version) == 0
        assert adm_repo_dataset.num_versions() == len(jmeta["_VERSIONS"])
        assert adm_repo_dataset.get_version_names() == jmeta["_VERSIONS"]

def test_admrepodataset_from_dataset():
        """Test the AutodatamanRepoDatasetMD.set_from_admdataset() method."""
        strRepoPath = "./test_repo"
        strDataset = "test_dataset"
        fVerbose = False
        
        adm_repo_dataset_1 = AutodatamanRepoDatasetMD()
        adm_repo_dataset_1.from_local_repo(strRepoPath,strDataset,fVerbose)

        adm_repo_dataset_2 = AutodatamanRepoDatasetMD()
        adm_repo_dataset_2.set_from_admdataset(adm_repo_dataset_1)

        # Check that the two admrepodatasets match
        assert adm_repo_dataset_2.get_default_version() == adm_repo_dataset_1.get_default_version()
        assert adm_repo_dataset_2.get_short_name() == adm_repo_dataset_1.get_short_name()
        assert adm_repo_dataset_2.get_source() == adm_repo_dataset_1.get_source()

def test_admrepodataset_to_file():
        """Test the AutodatamanRepoDatasetMD.to_file() method, which writes a metadata file."""
        strRepoPath = "./test_repo"
        strDataset = "test_dataset"
        fVerbose = False
        
        adm_repo_dataset = AutodatamanRepoDatasetMD()
        adm_repo_dataset.from_local_repo(strRepoPath,strDataset,fVerbose)

        test_file_path = "./tmp_test_file"
        adm_repo_dataset.to_file(test_file_path)

        # Check that output file was successfully written
        assert os.path.exists(test_file_path)
        with open(test_file_path,"r") as ifs:
                jmeta = json.load(ifs)
        # Clean up before doing any more tests
        os.remove(test_file_path)

        assert isinstance(jmeta,dict)
        assert "_DATASET" in jmeta
        assert "_VERSIONS" in jmeta
        
        # Check that adm object matches contents of output file
        assert adm_repo_dataset.m_strShortName == jmeta["_DATASET"]["short_name"]
        assert adm_repo_dataset.m_strLongName == jmeta["_DATASET"]["long_name"]
        assert adm_repo_dataset.m_strSource == jmeta["_DATASET"].get("source","")
        assert adm_repo_dataset.m_strDefaultVersion == jmeta["_DATASET"].get("default","")
        assert adm_repo_dataset.get_version_names() == jmeta["_VERSIONS"]

def test_admrepodataset_add_version():
        """Test the AutodatamanRepoDatasetMD.add_version() method."""
        strRepoPath = "./test_repo"
        strDataset = "test_dataset"
        fVerbose = False
        
        adm_repo_dataset = AutodatamanRepoDatasetMD()
        adm_repo_dataset.from_local_repo(strRepoPath,strDataset,fVerbose)

        adm_repo_dataset.add_version("v2")

        assert adm_repo_dataset.find_version("v2") == 1

def test_admrepodataset_remove_version():
        """Test the AutodatamanRepoDatasetMD.remove_version() method."""
        strRepoPath = "./test_repo"
        strDataset = "test_dataset"
        fVerbose = False
        
        adm_repo_dataset = AutodatamanRepoDatasetMD()
        adm_repo_dataset.from_local_repo(strRepoPath,strDataset,fVerbose)

        adm_repo_dataset.add_version("v2")
        adm_repo_dataset.remove_version("v1")

        assert adm_repo_dataset.find_version("v2") == 0

def test_namelist():
        """Testing all the namelist functionality."""
        nml = Namelist()
        strFile = "./autodataman"
        nml.FromFile(strFile)

        with open(strFile,"r") as ifs:
                jmeta = json.load(ifs)
        assert nml["default_local_repo"] == jmeta["default_local_repo"]

        test_value = "test_value"
        nml["test_setting"] = test_value
        assert nml["test_setting"] == test_value

        strFile = "./namelist"
        nml.ToFile(strFile)
        assert os.path.exists(strFile)
        with open(strFile,"r") as ifs:
                jmeta = json.load(ifs)
        os.remove(strFile)
        assert "default_local_repo" in jmeta
        assert nml["default_local_repo"] == jmeta["default_local_repo"]

def test_AutodatamanNamelist(monkeypatch):
        def mockreturn():
                return Path("test_home_read")
        
        monkeypatch.setattr(Path, "home", mockreturn)

        adm_nml = AutodatamanNamelist()
        adm_nml.LoadFromUser()

        with open(str(mockreturn()/".autodataman"),"r") as ifs:
                jmeta = json.load(ifs)
        assert adm_nml["default_local_repo"] == jmeta["default_local_repo"]

def test_get_dataset_name_version():
        dataset_name, dataset_version = get_dataset_name_version("test_dataset_1")
        assert dataset_name == "test_dataset_1"
        assert dataset_version == ""

        dataset_name, dataset_version = get_dataset_name_version("test_dataset_1/v1")
        assert dataset_name == "test_dataset_1"
        assert dataset_version == "v1"

def test_adm_getrepo_string(monkeypatch):
        def mockreturn():
                return Path("test_home_read")
        
        monkeypatch.setattr(Path, "home", mockreturn)

        test_str = adm_getrepo_string()
        with open(str(mockreturn()/".autodataman"),"r") as ifs:
                jmeta = json.load(ifs)
        assert test_str == jmeta["default_local_repo"]

def test_adm_getserver_string(monkeypatch):
        def mockreturn():
                return Path("test_home_read")
        
        monkeypatch.setattr(Path, "home", mockreturn)

        test_str = adm_getserver_string()
        with open(str(mockreturn()/".autodataman"),"r") as ifs:
                jmeta = json.load(ifs)
        assert test_str == jmeta["default_server"]

def test_adm_config_get(monkeypatch,capfd):
        def mockreturn():
                return Path("test_home_read")
        
        monkeypatch.setattr(Path, "home", mockreturn)
        adm_config_get()
        out, err = capfd.readouterr()
        # Just want to make sure something got printed
        assert len(out) > 1

def test_adm_config_set(monkeypatch):
        def mockreturn():
                return Path("test_home_readwrite")
        monkeypatch.setattr(Path, "home", mockreturn)
        adm_config_set("default_local_repo","test_value")
        with open(str(mockreturn()/".autodataman"),"r") as ifs:
                jmeta = json.load(ifs)
        assert jmeta["default_local_repo"] == "test_value"
        # reset the test file. TODO write this file in setup function?
        adm_config_set("default_local_repo","test_repo")

def test_adm_initrepo():
        default_json = {"_REPO": {
        "type": "autodataman",
        "version": "1"},
        "_DATASETS": []}
        adm_initrepo("test_repo_2")
        with open("test_repo_2/repo.json","r") as ifs:
                jmeta = json.load(ifs)
        assert os.path.exists("test_repo_2")
        assert os.path.exists("test_repo_2/repo.json")
        assert default_json == jmeta
        # Clean up
        shutil.rmtree("test_repo_2")

def test_adm_setrepo(monkeypatch):
        def mockreturn():
                return Path("test_home_readwrite")
        monkeypatch.setattr(Path, "home", mockreturn)
        with pytest.raises(FileNotFoundError) as e_info:
                adm_setrepo("nonexistant")
        adm_setrepo("test_repo")
        with open(str(mockreturn()/".autodataman"),"r") as ifs:
                jmeta = json.load(ifs)
        assert jmeta["default_local_repo"] == "test_repo"

def test_adm_getrepo(monkeypatch,capfd):
        def mockreturn():
                return Path("test_home_readwrite")
        monkeypatch.setattr(Path, "home", mockreturn)
        adm_getrepo()
        out, err = capfd.readouterr()
        assert len(out) > 1

def test_adm_setserver(requests_mock,monkeypatch,capfd):
        """Test the setserver function."""
        def mockreturn():
                return Path("test_home_readwrite")
        monkeypatch.setattr(Path, "home", mockreturn)

        strServer="http://test_admrepo_from_server.com"
        fVerbose = False

        mock_dict = return_repo_dot_json()
        requests_mock.get(strServer+"/repo.json",json=mock_dict)

        adm_setserver(strServer)
        out, err = capfd.readouterr()
        # Check that something got printed
        assert len(out) > 1

        # Check that default server in .autodataman file got written
        with open(str(mockreturn()/".autodataman"),"r") as ifs:
                jmeta = json.load(ifs)
        assert jmeta["default_server"] == strServer

def test_adm_getserver(monkeypatch,capfd):
        def mockreturn():
                return Path("test_home_read")
        monkeypatch.setattr(Path, "home", mockreturn)

        adm_getserver()
        out, err = capfd.readouterr()
        # Check that something got printed
        assert len(out) > 1

def test_adm_avail(requests_mock,monkeypatch,capfd):
        def mockreturn():
                return Path("test_home_read")
        monkeypatch.setattr(Path, "home", mockreturn)

        strServer="http://test_admrepo_from_server.com"
        fVerbose = False

        mock_dict = return_repo_dot_json()
        requests_mock.get(strServer+"/repo.json",json=mock_dict)

        adm_avail(strServer, fVerbose)
        out, err = capfd.readouterr()
        # Check that something got printed
        assert len(out) > 1

def test_adm_list(monkeypatch,capfd):
        def mockreturn():
                return Path("test_home_read")
        monkeypatch.setattr(Path, "home", mockreturn)

        strRepoPath = "./test_repo"
        fVerbose = False

        adm_list(strRepoPath, fVerbose)
        out, err = capfd.readouterr()
        # Check that something got printed
        assert len(out) > 1

def test_adm_info(requests_mock,monkeypatch,capfd):
        def mockreturn():
                return Path("test_home_read")
        monkeypatch.setattr(Path, "home", mockreturn)

        strServer="http://test_admrepo_from_server.com"
        strLocalRepo = "test_repo"
        strDataset = "test_dataset"
        fVerbose = False

        mock_dict = return_repo_dot_json()
        requests_mock.get(strServer+"/repo.json",json=mock_dict)

        mock_dict2 = return_dataset_dot_json()
        mock_text = str(mock_dict2).replace("'","\"")
        requests_mock.get(strServer+"/"+strDataset+"/dataset.json",text=mock_text)

        adm_info(strServer, strLocalRepo, strDataset, fVerbose)
        out, err = capfd.readouterr()
        # Check that something got printed
        assert len(out) > 1

def adm_remove_directory_variables():
        strLocalRepo = "test_repo"
        strDataset = "test_dataset_remove"
        strVersion = "v1"
        return strLocalRepo, strDataset, strVersion

@pytest.fixture(scope='function')
def setup_teardown_test_repo(request):
        strLocalRepo, strDataset, strVersion = adm_remove_directory_variables()
        dir_path = os.path.join(strLocalRepo,strDataset,strVersion)
        Path(dir_path).mkdir(parents=True)
        with open(strLocalRepo+"/"+strDataset+"/dataset.json","w") as ofs:
                json.dump(return_dataset_dot_json(),ofs)
        with open(dir_path+"/data.json","w") as ofs:
                json.dump(return_data_dot_json(), ofs)

        def teardown_directory():
                if os.path.exists(dir_path):
                        shutil.rmtree(dir_path)
                if os.path.exists(strLocalRepo+"/"+strDataset):
                        shutil.rmtree(strLocalRepo+"/"+strDataset)
        request.addfinalizer(teardown_directory)

def test_adm_remove_dataset(setup_teardown_test_repo):
        """Test adm_remove, which removes a dataset.
        fVerbose and fRemoveAll are set to False."""
        strLocalRepo, strDataset, strVersion = adm_remove_directory_variables()
        dir_path = os.path.join(strLocalRepo,strDataset,strVersion)

        adm_remove(strLocalRepo,strDataset,False,False)

        assert os.path.exists(os.path.join(strLocalRepo,strDataset)) == False
        assert os.path.exists(dir_path) == False

def test_adm_remove_version(setup_teardown_test_repo):
        """Test adm_remove, which removes a version from a dataset.
        fVerbose and fRemoveAll are set to False."""
        strLocalRepo, strDataset, strVersion = adm_remove_directory_variables()
        dir_path = os.path.join(strLocalRepo,strDataset,strVersion)

        adm_remove(strLocalRepo,strDataset+"/"+strVersion,False,False)

        # In this case, the version directory should be deleted, but not
        # the dataset directory
        assert os.path.exists(os.path.join(strLocalRepo,strDataset)) == True
        assert os.path.exists(dir_path) == False

@pytest.fixture(scope='function')
def setup_teardown_adm_get(request):
        strLocalRepo = "test_repo"
        strDataset = "test_dataset_2"

        def teardown_directory():
                dataset_path = strLocalRepo +"/"+strDataset
                if os.path.exists(dataset_path):
                        shutil.rmtree(dataset_path)
                # Clean up repo.json manually
                with open("test_repo/repo.json","r") as ifs:
                        jmeta = json.load(ifs)
                if "test_dataset_2" in jmeta["_DATASETS"]:
                        jmeta["_DATASETS"].remove("test_dataset_2")
                        with open("test_repo/repo.json","w") as ofs:
                                json.dump(jmeta, ofs, indent=4)
        request.addfinalizer(teardown_directory)

def test_adm_get_version(requests_mock,monkeypatch,setup_teardown_adm_get):
        """Test the adm_get function with version specified and verbose."""
        
        # Setup test home directory
        def mockreturn():
                return Path("test_home_read")
        monkeypatch.setattr(Path, "home", mockreturn)

        # Set up test localrepo
        strLocalRepo = "test_repo"
        strRemoteRepo = "test_remote_repo"

        # Set up test strserver
        strServer="http://test_admrepo_from_server.com"
        strVersion = "v1"
        strDataset = "test_dataset_2"
        fVerbose = True
        fForceOverwrite = True

        # mock repo.json using data from local repo
        remote_repo_path = strRemoteRepo+"/repo.json"
        with open(remote_repo_path,"r") as ifs:
                mock_dict = json.load(ifs)
        requests_mock.get(strServer+"/repo.json",json=mock_dict)

        # mock data.json using data from local repo
        remote_data_path = strRemoteRepo+"/"+strDataset+"/"+strVersion+"/data.json"
        with open(remote_data_path,"r") as ifs:
                mock_dict = json.load(ifs)
        requests_mock.get(strServer+"/"+strDataset+"/"+strVersion+"/data.json",json=mock_dict)

        # mock dataset.json using data from local repo
        remote_dataset_path = strRemoteRepo+"/"+strDataset+"/dataset.json"
        with open(remote_dataset_path,"r") as ifs:
                mock_dict = json.load(ifs)
        requests_mock.get(strServer+"/"+strDataset+"/dataset.json",json=mock_dict)

        # mock test dataset using data from local repo
        remote_data_path = strRemoteRepo +"/"+strDataset+"/" + strVersion + "/test_file_2.txt"
        with open(remote_data_path,"rb") as infile:
                mock_data = infile.read()
        mock_server_path = strServer+"/"+strDataset+"/" + strVersion + "/test_file_2.txt"
        requests_mock.get(mock_server_path,content=mock_data)

        # Running adm_get with a dataet and version provided
        adm_get(strServer, strLocalRepo, strDataset+"/v1", fForceOverwrite, fVerbose)

        # Check that data file exists in destination repo
        local_data_path = strLocalRepo +"/"+strDataset+"/" + strVersion + "/test_file_2.txt"
        assert os.path.exists(local_data_path)

        # Check that data.json exists in destination repo
        local_version_path = strLocalRepo +"/"+strDataset+"/" + strVersion + "/data.json"
        assert os.path.exists(local_version_path)

        # Check that dataset.json exists in destination repo
        local_dataset_path = strLocalRepo +"/"+strDataset+"/dataset.json"
        assert os.path.exists(local_dataset_path)

        # Check that destination repo.json was updated
        with open("test_repo/repo.json","r") as ifs:
                jmeta = json.load(ifs)
        assert "test_dataset_2" in jmeta["_DATASETS"]

def test_adm_get_dataset(requests_mock,monkeypatch,setup_teardown_adm_get):
        """Test the adm_get function using just a dataset argument."""
        
        # Setup test home directory
        def mockreturn():
                return Path("test_home_read")
        monkeypatch.setattr(Path, "home", mockreturn)

        # Set up test localrepo
        strLocalRepo = "test_repo"
        strRemoteRepo = "test_remote_repo"

        # Set up test strserver
        strServer="http://test_admrepo_from_server.com"
        strVersion = "v1"
        strDataset = "test_dataset_2"
        fVerbose = False
        fForceOverwrite = False

        # mock repo.json using data from local repo
        remote_repo_path = strRemoteRepo+"/repo.json"
        with open(remote_repo_path,"r") as ifs:
                mock_dict = json.load(ifs)
        requests_mock.get(strServer+"/repo.json",json=mock_dict)

        # mock data.json using data from local repo
        remote_data_path = strRemoteRepo+"/"+strDataset+"/"+strVersion+"/data.json"
        with open(remote_data_path,"r") as ifs:
                mock_dict = json.load(ifs)
        requests_mock.get(strServer+"/"+strDataset+"/"+strVersion+"/data.json",json=mock_dict)

        # mock dataset.json using data from local repo
        remote_dataset_path = strRemoteRepo+"/"+strDataset+"/dataset.json"
        with open(remote_dataset_path,"r") as ifs:
                mock_dict = json.load(ifs)
        requests_mock.get(strServer+"/"+strDataset+"/dataset.json",json=mock_dict)

        # mock test dataset using data from local repo
        remote_data_path = strRemoteRepo +"/"+strDataset+"/" + strVersion + "/test_file_2.txt"
        with open(remote_data_path,"rb") as infile:
                mock_data = infile.read()
        mock_server_path = strServer+"/"+strDataset+"/" + strVersion + "/test_file_2.txt"
        requests_mock.get(mock_server_path,content=mock_data)

        # Running adm_get with a dataset only provided
        adm_get(strServer, strLocalRepo, strDataset, fForceOverwrite, fVerbose)

        # Check that data file exists in destination repo
        local_data_path = strLocalRepo +"/"+strDataset+"/" + strVersion + "/test_file_2.txt"
        assert os.path.exists(local_data_path)

        # Check that data.json exists in destination repo
        local_version_path = strLocalRepo +"/"+strDataset+"/" + strVersion + "/data.json"
        assert os.path.exists(local_version_path)

        # Check that dataset.json exists in destination repo
        local_dataset_path = strLocalRepo +"/"+strDataset+"/dataset.json"
        assert os.path.exists(local_dataset_path)

        # Check that destination repo.json was updated
        with open("test_repo/repo.json","r") as ifs:
                jmeta = json.load(ifs)
        assert "test_dataset_2" in jmeta["_DATASETS"]
