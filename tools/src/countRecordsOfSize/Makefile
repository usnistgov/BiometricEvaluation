#
# This software was developed at the National Institute of Standards and
# Technology (NIST) by employees of the Federal Government in the course
# of their official duties. Pursuant to title 17 Section 105 of the
# United States Code, this software is not subject to copyright protection
# and is in the public domain. NIST assumes no responsibility whatsoever for
# its use by other parties, and makes no guarantees, expressed or implied,
# about its quality, reliability, or any other characteristic.
#
include ../common.mk
#
LIB = $(LD_BIOMEVAL)
#
SOURCES = countRecordsOfSize.cpp
OBJECTS = $(SOURCES:%.cpp=%.o)
#
PROGRAM = countRecordsOfSize

all: $(PROGRAM)

debug: CXXFLAGS += -g
debug: all

$(PROGRAM): $(OBJECTS)
	$(CXX) $(CXXFLAGS) $(OBJECTS) -o $@ $(LD_BIOMEVAL)
	test -d $(LOCALBIN) || mkdir $(LOCALBIN)
	$(CP) $@ $(LOCALBIN)/$@

clean:
	$(RM) $(PROGRAM) $(OBJECTS)
