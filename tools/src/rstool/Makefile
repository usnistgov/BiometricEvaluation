#
# This software was developed at the National Institute of Standards and
# Technology (NIST) by employees of the Federal Government in the course
# of their official duties. Pursuant to title 17 Section 105 of the
# United States Code, this software is not subject to copyright protection
# and is in the public domain. NIST assumes no responsibility whatsoever for
# its use by other parties, and makes no guarantees, expressed or implied,
# about its quality, reliability, or any other characteristic.
#
# Builds rstool, a tool for manipulating RecordStores.
#
#
# rstool versioning
# 
MAJOR_VERSION=1
MINOR_VERSION=4
#
include ../common.mk
#
SOURCES = image_additions.cpp lrs_additions.cpp rstool.cpp
OBJECTS = $(SOURCES:%.cpp=%.o)
#
PROGRAM = rstool

CXXFLAGS += -I.

all: $(PROGRAM)

debug: CXXFLAGS += -g
debug: all

# OS X needs Xquartz to be installed
ifeq ($(OS),Darwin)
LD_BIOMEVAL += -L/opt/local/lib
CXXFLAGS += -I/opt/local/include
endif

$(PROGRAM): $(OBJECTS)
	$(CXX) $(CXXFLAGS) $(OBJECTS) -o $@ $(LD_BIOMEVAL) -lX11
	test -d $(LOCALBIN) || mkdir $(LOCALBIN)
	$(CP) $@ $(LOCALBIN)/$@
	$(CP) $@.1 $(LOCALMAN)

rstool.o: rstool.cpp lrs_additions.cpp image_additions.cpp
	$(CXX) $(CXXFLAGS) -DMAJOR_VERSION=$(MAJOR_VERSION) -DMINOR_VERSION=$(MINOR_VERSION) -c $< -o $@

clean:
	$(RM) $(PROGRAM) $(OBJECTS)

