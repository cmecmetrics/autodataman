The autodataman tool is intended to be a very lightweight climate data database and data
distribution engine.  The following command line options are available as part of the tool.

autodataman config [<variable> <value>]
Set the specified configuration variable to the given value.

autodataman initrepo <local repo dir>
Initialize a local repository directory.

autodataman setrepo <local repo dir>
Set the default local repository directory.

autodataman getrepo
Get the current default local repository directory.

autodataman setserver <server>
Set the default data server URL.

autodataman getserver
Get the default data server URL.

autodataman avail [-v] [-s <server>]
List datasets available on the given remote server.

autodataman info [-v] [-l <local repo dir>] [-s <server>] <dataset id>
Give detailed information on the given dataset id.

autodataman list [-v] [-l <local repo dir>]
List datasets available on the local data server.

autodataman validate [-l <local repo dir>]
Validate the datasets on the local repo.

autodataman remove [-a] [-v] [-l <local repo dir>] <dataset id>[/version]
Remove a dataset from the local data server.

autodataman get [-f] [-v] [-l <local repo dir>]  [-s <server>] <dataset id>[/<version>]
Get the specified dataset from the remote data server and store it on the local data repo or remote server.

autodataman put [-l <local repo dir>] [-s <server>] [-c <compression type>] <dataset dir>/<version>
Put the specified dataset into the local repository or remote server.

