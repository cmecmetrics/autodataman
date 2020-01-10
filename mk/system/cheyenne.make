# Copyright (c) 2019 Paul Ullrich 
#
# Distributed under the Boost Software License, Version 1.0. (See accompanying 
# file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

# NERSC Babbage Testbed

CXX=               icpc
MPICXX=            icpc

LDFLAGS+= -Wl,-rpath,/glade/u/apps/opt/intel/2016u3/compilers_and_libraries/linux/mkl/lib/intel64

# DO NOT DELETE
