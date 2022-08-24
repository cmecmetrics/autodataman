#!/usr/bin/env python
from setuptools import find_packages, setup

packages=find_packages()

setup(
    name='autodataman',
    version='0.2',
    description='Autodataman',
    long_description='The autodataman tool is intended to be a very lightweight climate data database and data distribution engine.',
    url='https://github.com/cmecmetrics/autodataman.git',
    author='Paul Ullrich and Ana Ordonez',
    author_email='ordonez4@llnl.gov',
    classifiers=[
        'Development Status :: 4 - Beta',
        'Intended Audience :: Science/Research',
        'Topic :: Scientific/Engineering',
        'License :: OSI Approved :: BSD 3-Clause License',
        'Operating System :: MacOS',
        'Operating System :: POSIX',
        'Operating System :: POSIX :: Linux',
        'Programming Language :: Python :: 3',
    ],
    keywords=['benchmarking','earth system modeling','climate modeling','model intercomparison','data management'],
    packages=packages,
    entry_points={
        "console_scripts": [
            "autodataman=autodataman.autodataman:main",
            "adm=autodataman.autodataman:main"
        ]
    }
)
