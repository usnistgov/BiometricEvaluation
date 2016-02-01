# This software was developed at the National Institute of Standards and
# Technology (NIST) by employees of the Federal Government in the course
# of their official duties. Pursuant to title 17 Section 105 of the
# United States Code, this software is not subject to copyright protection
# and is in the public domain. NIST assumes no responsibility whatsoever for
# its use by other parties, and makes no guarantees, expressed or implied,
# about its quality, reliability, or any other characteristic.

include ../../../VERSION

.PHONY: all build clean install rcpp uninstall

R_EXE ?= /usr/nistlocal/R/bin/R
RSCRIPT_EXE ?= /usr/nistlocal/R/bin/Rscript
PACKAGE := RecordStore

all: rcpp

rcpp:
	$(shell cp $(PACKAGE)/DESCRIPTION.mk $(PACKAGE)/DESCRIPTION)
	$(shell echo "Version: $(MAJOR_VERSION).$(MINOR_VERSION)-$(COMPATIBILITY_VERSION)" >> $(PACKAGE)/DESCRIPTION)
	$(RSCRIPT_EXE) -e "library(Rcpp); compileAttributes(\"$(PACKAGE)\")"

build:
	$(R_EXE) CMD build $(PACKAGE)

install:
	$(R_EXE) CMD INSTALL $(PACKAGE)

uninstall:
	$(R_EXE) CMD REMOVE $(PACKAGE)

clean:
	$(RM) $(PACKAGE)/DESCRIPTION
	$(RM) $(PACKAGE)/src/*.o
	$(RM) $(PACKAGE)/src/*.so
	$(RM) $(PACKAGE)/R/RcppExports.R
	$(RM) $(PACKAGE)/src/RcppExports.cpp
	$(RM) $(PACKAGE)_*.tar.gz
	
