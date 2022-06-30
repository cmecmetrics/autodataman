# Autodataman

The autodataman tool is intended to be a very lightweight climate data database and data distribution engine.  

## Install

Clone the autodataman repository.

Enter the autodataman repository.

With conda active, run the following commands to create a new autodataman environment:
```
conda create -n adm -c conda-forge python=3.7 requests
conda activate adm
pip install .
```

To install autodataman in an existing environment, use the following:
```
conda activate your_environment
pip install .
```

The requests library is a dependency that must also be installed in your existing environment.

## Command line
The following command line options are available as part of the tool.

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

autodataman remove [-a] [-v] [-l \<local repo dir\>] \<dataset id\>[/version]  
Remove a dataset from the local data server.

autodataman get [-f] [-v] [-l \<local repo dir\>]  [-s \<server\>] \<dataset id\>[/\<version\>]  
Get the specified dataset from the remote data server and store it on the local data repo or remote server.

