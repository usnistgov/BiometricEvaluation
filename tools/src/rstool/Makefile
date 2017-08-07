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

.PHONY: all clean debug export install

LOCALBIN := ../../bin
LOCALMAN := ../../man/man1
CP := cp -f

MAJOR_VERSION = $(shell grep MAJOR_VERSION VERSION | awk -F= '{print $$2}')
MINOR_VERSION = $(shell grep MINOR_VERSION VERSION | awk -F= '{print $$2}')
PACKAGE_DIR = rstool-$(MAJOR_VERSION).$(MINOR_VERSION)

SOURCES = image_additions.cpp lrs_additions.cpp rstool.cpp
OBJECTS = $(SOURCES:%.cpp=%.o)
PROGRAM = rstool

CXXFLAGS += -I. -Wall -pedantic -std=c++11

BIOMEVAL_CXXFLAGS = -I../../../common/src/include
BIOMEVAL_LDFLAGS = ../../../common/lib/libbiomeval.a $(shell pkg-config --libs sqlite3) $(shell pkg-config --static --libs libpng)  $(shell pkg-config --libs libopenjp2)  $(shell pkg-config --libs libcrypto) $(shell pkg-config --libs x11) $(shell pkg-config --libs libtiff-4) -ljpeg

CXXFLAGS += $(BIOMEVAL_CXXFLAGS) $(shell pkg-config --cflags x11)
LDFLAGS += $(BIOMEVAL_LDFLAGS)

OS := $(shell uname -s)
ifeq ($(OS),Darwin)
# macOS needs libdb from MacPorts
LDFLAGS += -L/opt/local/lib/db44 -ldb
# macOS needs frameworks when statically linking against libbiomeval.a
LDFLAGS += -framework Foundation -framework Security -framework PCSC
else
LDFLAGS += -ldb
endif

all: $(PROGRAM)

debug: CXXFLAGS += -g
debug: all

$(PROGRAM): $(OBJECTS)
	$(CXX) $(CXXFLAGS) $(OBJECTS) -o $@ $(LDFLAGS)

install:
	test -d $(LOCALBIN) || mkdir -p $(LOCALBIN)
	test -d $(LOCALMAN) || mkdir -p $(LOCALMAN)
	$(CP) $(PROGRAM) $(LOCALBIN)/$(PROGRAM)
	$(CP) $(PROGRAM).1 $(LOCALMAN)

rstool.o: rstool.cpp lrs_additions.cpp image_additions.cpp
	$(CXX) $(CXXFLAGS) -DMAJOR_VERSION=$(MAJOR_VERSION) -DMINOR_VERSION=$(MINOR_VERSION) -c $< -o $@
	
image_additions.o: CXXFLAGS += -Wno-variadic-macros

clean:
	$(RM) $(PROGRAM) $(OBJECTS) $(PACKAGE_DIR).tar.gz
	$(RM) -r $(PACKAGE_DIR)

export: clean
	mkdir -p $(PACKAGE_DIR)
	cd $(PACKAGE_DIR) && ln -s	\
	    ../VERSION			\
	    ../*.h			\
	    ../*.cpp			\
	    ../rstool.1			\
            .
	cd $(PACKAGE_DIR) && ln -s ../Makefile.rpm Makefile
	tar czfh $(PACKAGE_DIR).tar.gz $(PACKAGE_DIR)
	$(RM) -r $(PACKAGE_DIR)

