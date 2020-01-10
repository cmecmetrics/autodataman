# Copyright (c) 2019 Paul Ullrich 
#
# Distributed under the Boost Software License, Version 1.0. (See accompanying 
# file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)

# Base directory.
AUTODATAMANDIR=..

# Load Makefile framework. 
include $(AUTODATAMANDIR)/mk/framework.make

BUILD_TARGETS+= base contrib
CLEAN_TARGETS= $(addsuffix .clean,$(BUILD_TARGETS))

AUTODATAMANLDFLAGS+= -L$(AUTODATAMANDIR)/src/base -L$(AUTODATAMANDIR)/src/contrib

LIBRARIES+= -lautodatamanbase -lautodatamancontrib

EXEC_FILES= autodataman.cpp

EXEC_TARGETS= $(EXEC_FILES:%.cpp=%)

FILES= $(UTIL_FILES) $(EXEC_FILES)

.PHONY: all clean $(BUILD_TARGETS)

# Build rules. 
all: $(BUILD_TARGETS) $(EXEC_TARGETS)

$(BUILD_TARGETS): %:
	cd $*; $(MAKE)

$(EXEC_TARGETS): %: $(BUILD_TARGETS) $(BUILDDIR)/%.o $(AUTODATAMANLIBS)
	$(CXX) $(LDFLAGS) $(AUTODATAMANLDFLAGS) -o $@ $(UTIL_FILES:%.cpp=$(BUILDDIR)/%.o) $(BUILDDIR)/$*.o $(LIBRARIES)
	mv $@ $(AUTODATAMANDIR)/bin

# Clean rules.
clean: $(CLEAN_TARGETS)
	rm -rf $(DEPDIR)
	rm -rf $(BUILDDIR)

$(CLEAN_TARGETS): %.clean:
	cd $*; $(MAKE) clean

# Include dependencies.
-include $(FILES:%.cpp=$(DEPDIR)/%.d)

# DO NOT DELETE
