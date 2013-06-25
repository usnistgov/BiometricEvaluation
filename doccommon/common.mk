# This software was developed at the National Institute of Standards and
# Technology (NIST) by employees of the Federal Government in the course
# of their official duties. Pursuant to title 17 Section 105 of the
# United States Code, this software is not subject to copyright protection
# and is in the public domain. NIST assumes no responsibility whatsoever for
# its use by other parties, and makes no guarantees, expressed or implied,
# about its quality, reliability, or any other characteristic.

#
# Override executables
#
LATEXMK = $(shell which latexmk) -pdf 
DOXYGEN = $(shell which doxygen)

#
# Check if doxygen generated documentation should be appended to this
# document.
#
DOXYCONFIG = $(ROOTNAME).dox
GENERATEDOXYGEN = $(shell test -f $(DOXYCONFIG) && echo YES || echo NO)

#
# Check if there's a glossary to be included
#
GLOSSARY = glossary.tex
GENERATEGLOSSARY = $(shell test -f $(GLOSSARY) && echo YES || echo NO)

# Common Images
SOURCEIMAGES = $(LOCALINC)/assets/*

# Common Bibliography
COMMONBIB = $(LOCALINC)/common.bib
SOURCEBIB = $(ROOTNAME).bib

#
# Setup the build directory structure, working around the doxygen defaults.
#
BUILDDIR = build
LATEXBUILDDIR = $(BUILDDIR)/latex
HTMLBUILDDIR = $(BUILDDIR)/html
ifeq ($(GENERATEDOXYGEN), YES)
LATEXROOTNAME = refman
else
LATEXROOTNAME = $(ROOTNAME)
endif
LATEXPDF = $(LATEXROOTNAME).pdf
LATEXBIB = $(LATEXROOTNAME).bib
LATEXMAIN = $(ROOTNAME).tex
PDFMAIN = $(ROOTNAME).pdf


#
# Check whether the main PDF file exists in the top-level doc directory, and
# whether it's writable. If not present, we'll just create it; if present, and
# not writable, error out. These checks accomodate having the main PDF file
# kept under version control where the local write permission is controlled
# by version control, e.g. Perforce.
#
PDFMAINPRESENT = $(shell test -f $(PDFMAIN) && echo yes)
ifeq ($(PDFMAINPRESENT),yes)
	PDFMAINWRITABLE = $(shell test -w $(PDFMAIN) && echo yes)
else
	PDFMAINWRITABLE = yes
endif

all: build $(PDFMAIN) 
build:
	mkdir -p $(LATEXBUILDDIR)
	mkdir -p $(HTMLBUILDDIR)

$(PDFMAIN): $(LATEXBUILDDIR)/$(LATEXPDF)
ifneq ($(PDFMAINWRITABLE),yes)
	$(error $(PDFMAIN) is not writable)
else
	cp -p $(LATEXBUILDDIR)/$(LATEXPDF) $(PDFMAIN)
endif

#
# The LaTeX build is a bit complicated, because Doxygen names the
# LaTeX related files to start with 'refman'. In order to keep the
# bibliography file named based on our document name, it is copied
# into the build directory as refman.bib, but linked to the name we
# want.
# Also copy the non-Doxygen generated source files to the build directory.
#
ifeq ($(GENERATEDOXYGEN), YES)
$(LATEXBUILDDIR)/$(LATEXPDF): $(SOURCECODE) $(SOURCETEX) $(LATEXMAIN) $(DOXYCONFIG)
else
$(LATEXBUILDDIR)/$(LATEXPDF): $(SOURCECODE) $(SOURCETEX) $(LATEXMAIN)
endif
# Combined project-level bibliography files with the common references
	cat $(SOURCEBIB) $(LOCALINC)/shared.bib > $(LATEXBUILDDIR)/$(LATEXBIB)
ifeq ($(GENERATEDOXYGEN), YES)
endif
	cp -f $(SOURCETEX) $(LATEXBUILDDIR)
	cp -f $(SOURCEIMAGES) $(LATEXBUILDDIR)
ifeq ($(GENERATEGLOSSARY), YES)
	cp -f $(GLOSSARY) $(LATEXBUILDDIR)
	ln -s $(GLOSSARY) $(LATEXBUILDDIR)/$(basename $GLOSSARY).glo
	cp $(LOCALINC)/latexmkrc $(LATEXBUILDDIR)
endif
ifeq ($(GENERATEDOXYGEN), YES)
# Copy project bibliography into refman bib
	ln -s $(LATEXBIB) $(LATEXBUILDDIR)/$(SOURCEBIB)
# Create body links to doxygen content
	$(foreach texdoc, $(SOURCETEX), perl -pe 's|(\\.*?doxyref){(.*?)}({([1-9]*?)})?|`scripts/$$1 $$2 $$4`|ge' -i $(LATEXBUILDDIR)/$(texdoc);)
# Generate doxygen content
	$(DOXYGEN) $(DOXYCONFIG)
endif
	cd $(LATEXBUILDDIR) && $(LATEXMK) $(LATEXROOTNAME)

clean:
ifeq ($(shell test -f $(LATEXBUILDDIR) && echo "YES" || echo "NO"), YES)
	cd $(LATEXBUILDDIR) && $(LATEXMK) -c $(LATEXROOTNAME).tex
endif
	$(RM) -r $(BUILDDIR)
