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
MAJOR_VERSION=2
MINOR_VERSION=0
#
include ../common.mk
#
SOURCES = image_additions.cpp lrs_additions.cpp rstool.cpp
OBJECTS = $(SOURCES:%.cpp=%.o)
#
PROGRAM = rstool

CXXFLAGS += -I. -I../../../common/src/include -Wall -pedantic -g
LDFLAGS += ../../../common/lib/libbiomeval.a -lz -lsqlite3 -lpng -lopenjp2 -lcrypto -lX11 -ljpeg
ifeq ($(OS),Darwin)
LDFLAGS += -L/opt/local/lib/db44 -ldb
else
LDFLAGS += -ldb
endif

all: $(PROGRAM)

debug: CXXFLAGS += -g
debug: all

# OS X needs Xquartz to be installed
ifeq ($(OS),Darwin)
LDFLAGS += -L/opt/local/lib -L/usr/X11/lib
CXXFLAGS += -I/opt/local/include -I/usr/X11/include
endif

$(PROGRAM): $(OBJECTS)
	$(CXX) $(CXXFLAGS) $(OBJECTS) -o $@ $(LDFLAGS)
	test -d $(LOCALBIN) || mkdir $(LOCALBIN)
	$(CP) $@ $(LOCALBIN)/$@
	$(CP) $@.1 $(LOCALMAN)

rstool.o: rstool.cpp lrs_additions.cpp image_additions.cpp
	$(CXX) $(CXXFLAGS) -DMAJOR_VERSION=$(MAJOR_VERSION) -DMINOR_VERSION=$(MINOR_VERSION) -c $< -o $@
	
image_additions.o: CXXFLAGS += -Wno-variadic-macros

clean:
	$(RM) $(PROGRAM) $(OBJECTS)

