# Autodataman

The autodataman tool is intended to be a very lightweight climate data database and data distribution engine.  

## Install

### Install from conda-forge (recommended)

Create a new conda environment that installs "autodataman" from the conda-forge channel. For example:
```
conda create -n adm -c conda-forge autodataman
```
To create an environment with cmec-driver and autodataman:
```
conda create -n cmec -c conda-forge cmec_driver autodataman
```

### Install from "main" with pip
Clone the autodataman repository.

Enter the autodataman repository.

With conda active, run the following commands to create a new autodataman environment:
```
conda create -n adm -c conda-forge python=3.10 requests
conda activate adm
pip install .
```

To install autodataman in an existing environment, use the following:
```
conda activate your_environment
pip install .
```

The requests library is a dependency that must also be installed in your existing environment.

### Install and test
Clone the autodataman repository and enter the repository. With conda active, run the following commands to create a new autodataman test environment:
```
conda create -n adm_dev -c conda-forge python=3.7 requests pytest
conda activate adm_dev
pip install .
pip install requests-mock
```

To run tests, enter the "tests" repository and run `pytest`.

## Command line
The following command line options are available as part of the tool. The alias "adm" can be used in place of "autodataman" for brevity.

autodataman config [\<variable\> \<value\>]  
Set the specified configuration variable to the given value.

autodataman initrepo \<local repo dir\>  
Initialize a local repository directory.

autodataman setrepo \<local repo dir\>  
Set the default local repository directory.

autodataman getrepo  
Get the current default local repository directory.

autodataman setserver \<server\>  
Set the default data server URL.

autodataman getserver  
Get the default data server URL.

autodataman avail [-v] [-s \<server\>]  
List datasets available on the given remote server.

autodataman info [-v] [-l \<local repo dir\>] [-s \<server\>] \<dataset id\>  
Give detailed information on the given dataset id.

autodataman list [-v] [-l \<local repo dir\>]  
List datasets available on the local data server.

autodataman remove [-a] [-v] [-l \<local repo dir\>] \<datas#  id\>[/version]  
Remove a dataset from the local data server.

autodataman get [-f] [-v] [-l \<local repo dir\>]  [-s \<server\>] \<dataset id\>[/\<version\>]  
Get the specified dataset from the remote data server and store it on the local data repo or remote server.

## Setting up an autodataman repository

Here is an example of the directory structure for an autodataman repository. This is considered a "Version 1" autodataman structure.  

```
repository_root
| repo.json
|
|---- dataset 1
|         | dataset.json
|         |
|         ----version 1
|         |       | data.json
|         |       | data.(nc,tar,txt,etc)
|         |   
|         ----version 2
|                 | data.json
|                 | data.(nc,tar,txt,etc)
|
|----dataset 2
|       | ...
```

### Metadata files

These JSON files contain basic information about the datasets being distributed. It is preferred that they use the .json extension. However, if you cannot share JSON files, you may use a ".txt" extension (using JSON syntax within the text file). All metadata file extensions will be converted to .json in local repositories.

repo.json   
"_REPO": Values set by autodataman software. Key-value pairs are "type":"autodataman" and "version":"1".  
"_DATASETS": A list of strings representing the names of datasets in the repository.  

Example of repo.json:  
```
{
    "_REPO": {
        "type": "autodataman",
        "version": "1"
    },
    "_DATASETS": [
        "IBTrACS"
    ]
}
```

dataset.json  
"_DATASET": Required keys are "short_name", "long_name", "source", and "default" (for default version).  
"_VERSIONS": A list of strings representing the versions of this dataset present in the repository.  

Example of dataset.json:  
```
{
    "_DATASET": {
        "short_name": "IBTrACS",
        "long_name": "International Best Track Archive for Climate Stewardship (IBTrACS)",
        "source": "",
        "default": "v04r00"
    },
    "_VERSIONS": [
        "v04r00"
    ]
}
```

data.json  
"_DATA": Required keys are "version", "date", and "source".  
"_FILES": A list of objects, where each object represents a file present in this version directory. The required keys in each object are "filename", "SHA256sum", and "format" (the file extension). An optional key is "on_download", containing commands that should be automatically executed on the file after download.  

Example of data.json:  
```
{
    "_DATA": {
        "version": "v04r00",
        "date": "2019-03-11",
        "source": "https://www.ncdc.noaa.gov/ibtracs/index.php"
    },
    "_FILES": [
        {
            "filename": "IBTrACS.tgz",
            "SHA256sum": "cfe1f7b113e1c77fa8c81c021f215f7234a8189ee45ef270569c72bb7eb76956",
            "format": "tgz",
            "on_download": "open"
        }
    ]
}
```

## License
Autodataman is distributed under the terms of the BSD 3-Clause License.  
LLNL-CODE-842430  

## Acknowledgement
Content in this repository is developed by climate and computer scientists from the Program for Climate Model Diagnosis and Intercomparison ([PCMDI][PCMDI]) at Lawrence Livermore National Laboratory ([LLNL][LLNL]). This work is sponsored by the Regional and Global Model Analysis ([RGMA][RGMA]) program, of the Earth and Environmental Systems Sciences Division ([EESSD][EESSD]) in the Office of Biological and Environmental Research ([BER][BER]) within the [Department of Energy][DOE]'s [Office of Science][OS]. The work is performed under the auspices of the U.S. Department of Energy by Lawrence Livermore National Laboratory under Contract DE-AC52-07NA27344.  

[PCMDI]: https://pcmdi.llnl.gov/
[LLNL]: https://www.llnl.gov/
[RGMA]: https://climatemodeling.science.energy.gov/program/regional-global-model-analysis
[EESSD]: https://science.osti.gov/ber/Research/eessd
[BER]: https://science.osti.gov/ber
[DOE]: https://www.energy.gov/
[OS]: https://science.osti.gov/


<p>
    <img src="https://github.com/PCMDI/assets/blob/main/DOE/480px-DOE_Seal_Color.png?raw=true"
         width="65"
         style="margin-right: 30px"
         title="United States Department of Energy"
         alt="United States Department of Energy"
    >&nbsp;
    <img src="https://github.com/PCMDI/assets/blob/main/LLNL/212px-LLNLiconPMS286-WHITEBACKGROUND.png?raw=true"
         width="65"
         title="Lawrence Livermore National Laboratory"
         alt="Lawrence Livermore National Laboratory"
    >
</p>
