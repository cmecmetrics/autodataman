import os
import hashlib
import requests

def download_sample_data_files(files_hash, path=None, hash_type="sha256"):
    """Downloads sample data from a list of files
    Default download directory is the current working directory.

    Example Usage:
        from autodataman.tools import *
        download_sample_data_files("data_files.txt",path="demo_data",hash_type="md5")

    Arguments:
    files_hash: Name of text file with hashes and file names. 
            The first line of the file is the download root url.
    path: String of a valid filepath or None.
        If None, sample data will be downloaded into the 
                current working directory.
    hash_type: String of the hash type used in files_hash.
                Default is 'sha256'.

    This function was taken from the cdat_info package, with some updates.
    cdat_info Copyright (c) 2018, Lawrence Livermore National Security, LLC
    """
    if not os.path.exists(files_hash) or os.path.isdir(files_hash):
        raise RuntimeError(
            "Invalid file type for list of files: %s" %
            files_hash)
    if path is None:
        path = os.getcwd()
    if hash_type not in hashlib.algorithms_available:
        raise RuntimeError("Hash type '"+str(hash_type)+"' not available in this interpreter.")
    samples = open(files_hash).readlines()
    download_url_root = samples[0].strip()
    for sample in samples[1:]:
        good_hash, name = sample.split()
        local_filename = os.path.join(path, name)
        try:
            os.makedirs(os.path.dirname(local_filename))
        except BaseException:
            pass
        attempts = 0
        while attempts < 3:
            file_hash = eval("hashlib.{0}()".format(hash_type))
            if os.path.exists(local_filename):
                f = open(local_filename, "rb")
                file_hash.update(f.read())
                if file_hash.hexdigest() == good_hash:
                    attempts = 5
                    continue
            print("Downloading: '%s' from '%s' in: %s" %
                  (name, download_url_root, local_filename))
            r = requests.get(
                "%s/%s" % (download_url_root, name),
                stream=True)
            with open(local_filename, 'wb') as f:
                for chunk in r.iter_content(chunk_size=1024):
                    if chunk:  # filter local_filename keep-alive new chunks
                        f.write(chunk)
                        file_hash.update(chunk)
            f.close()
            if file_hash.hexdigest() == good_hash:
                attempts = 5
            else:
                attempts += 1
