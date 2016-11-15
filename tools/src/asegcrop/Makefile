# This software was developed at the National Institute of Standards and
# Technology (NIST) by employees of the Federal Government in the course
# of their official duties. Pursuant to title 17 Section 105 of the
# United States Code, this software is not subject to copyright protection
# and is in the public domain. NIST assumes no responsibility whatsoever for
# its use by other parties, and makes no guarantees, expressed or implied,
# about its quality, reliability, or any other characteristic.
#
# Builds asegcrop: crop AN2K slap imagery with ASEG coordinates
#
.PHONY: all clean debug

include ../common.mk

PROGRAM := asegcrop
SOURCES := $(PROGRAM).cpp
OBJECTS := $(SOURCES:%.cpp=%.o)
CXXFLAGS += -std=c++11 -Wall -pedantic -I. -I../../../common/src/include -O3
LDFLAGS += ../../../common/lib/libbiomeval.a -lz -lsqlite3 -lpng -lopenjp2 -lcrypto -lX11 -ljpeg -lopencv_core -lopencv_imgproc -lopencv_highgui
ifeq ($(OS),Darwin)
        LDFLAGS += -L/opt/local/lib/db44 -ldb -L/opt/local/lib
        CXXFLAGS += -I/opt/local/include
else
        LDFLAGS += -ldb -lpng
endif

all: $(PROGRAM)

debug: CXXFLAGS += -g
debug: all

$(PROGRAM): $(OBJECTS)
	$(CXX) $(CXXFLAGS) $(OBJECTS) -o $@ $(LDFLAGS)
	test -d $(LOCALBIN) || mkdir $(LOCALBIN)
	$(CP) $@ $(LOCALBIN)/$@

clean:
	$(RM) $(OBJECTS) $(PROGRAM)
	$(RM) -r $(PROGRAM).dSYM

