# This software was developed at the National Institute of Standards and
# Technology (NIST) by employees of the Federal Government in the course
# of their official duties. Pursuant to title 17 Section 105 of the
# United States Code, this software is not subject to copyright protection
# and is in the public domain. NIST assumes no responsibility whatsoever for
# its use by other parties, and makes no guarantees, expressed or implied,
# about its quality, reliability, or any other characteristic.

#
# To create a Makefile for a LaTeX document:
#     - Define the following make variables:
#	- ROOTNAME: Name of the document
#	- SOURCETEX: TeX files that comprise the document
#	- LOCALINC: Relative path to 'doccommon'
#	- .DEFAULT_GOAL: make goal that calls thedoc
#
#     - Optional defines:
#	- COMMONTEX: .tex files (e.g. commonglossary) that the document will use
#	- COMMONASSETS: Common figures or other non-tex items.
#
#     - Create a make goal that makes 'thedoc'
#	.DEFAULT_GOAL := all
#	- all:
#		$(MAKE) thedoc
#
#     - If arguments to pdflatex are needed, append "-pdflatex="pdflatex <args>"
#	to $(LATEXMK)
#
# To add doxygen documentation:
#     - List the source code to include in SOURCECODE make variable
#
#     - include doccommon/common.mk
#
#     - If doxygen is to be run and the doxygen configuration file is not 
#	$(ROOTNAME).tex, define DOXYCONFIG and set GENERATEDOXYGEN to YES
#
# Good luck.
#

#
# Override executables
#
LATEXMK = $(shell which latexmk) -pdf -recorder
DOXYGEN = $(shell which doxygen)

#
# Check if doxygen generated documentation should be appended to this
# document.
#
DOXYCONFIG = $(ROOTNAME).dox
GENERATEDOXYGEN = $(shell test -f $(DOXYCONFIG) && echo YES || echo NO)

#
# Check if there's a common glossary to be included
#
GLOSSARY = commonglossary.tex

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
# If the common glossary is wanted, we need to tell latexmk to use it
# later on.
#
GENERATEGLOSSARY=$(if $(findstring $(GLOSSARY),$(COMMONTEX)),YES,NO)

#
# Check whether the main PDF file exists in the top-level doc directory, and
# whether it's writable. If not present, we'll just create it; if present, and
# not writable, error out. These checks accomodate having the main PDF file
# kept under version control where the local write permission is controlled
# by version control, e.g. Perforce.
#
PDFMAINPRESENT = $(shell test -f $(PDFMAIN) && echo YES)
ifeq ($(PDFMAINPRESENT),YES)
	PDFMAINWRITABLE = $(shell test -w $(PDFMAIN) && echo YES)
else
	PDFMAINWRITABLE = YES
endif

thedoc: build $(PDFMAIN)
# Error if main Makefile does not define a default goal
ifeq ($(.DEFAULT_GOAL), )
	$(error DEFAULT_GOAL is not set)
endif

build:
# Make directory structure
	mkdir -p $(LATEXBUILDDIR)
	mkdir -p $(HTMLBUILDDIR)
# Combined project-level bibliography files with the common references
	cat $(SOURCEBIB) $(LOCALINC)/shared.bib > $(LATEXBUILDDIR)/$(LATEXBIB)
ifneq ($(SOURCEBIB), $(LATEXBIB))
	ln -s $(LATEXBIB) $(LATEXBUILDDIR)/$(SOURCEBIB)
endif
	cp -f $(SOURCETEX) $(LATEXBUILDDIR)
# Are any common TeX files included?
ifdef COMMONTEX
	cp -f $(COMMONTEX) $(LATEXBUILDDIR)
endif
# Copy any assets into the build directory
ifdef COMMONASSETS
	cp -f $(COMMONASSETS) $(LATEXBUILDDIR)
endif
# Add rule to generate glossary, if present
ifeq ($(GENERATEGLOSSARY), YES)
	ln -s $(LATEXBUILDDIR)/$(GLOSSARY) $(LATEXBUILDDIR)/$(basename $(GLOSSARY)).glo
	cp $(LOCALINC)/latexmkrc $(LATEXBUILDDIR)
endif

$(PDFMAIN): $(LATEXBUILDDIR)/$(LATEXPDF)
ifneq ($(PDFMAINWRITABLE),YES)
	$(error $(PDFMAIN) is not writable)
else
	cp -p $(LATEXBUILDDIR)/$(LATEXPDF) $(PDFMAIN)
endif

# Build relies on doxygen config, if present
ifeq ($(GENERATEDOXYGEN), YES)
$(LATEXBUILDDIR)/$(LATEXPDF): $(SOURCECODE) $(SOURCETEX) $(LATEXMAIN) $(DOXYCONFIG)
else
$(LATEXBUILDDIR)/$(LATEXPDF): $(SOURCECODE) $(SOURCETEX) $(LATEXMAIN)
endif
	cd $(LATEXBUILDDIR) && $(LATEXMK) $(LATEXROOTNAME)

%.dox: build
# Rename main TeX file as refman for doxygen
	cp $(ROOTNAME).tex $(LATEXBUILDDIR)/$(LATEXROOTNAME).tex
	chmod +w $(LATEXBUILDDIR)/$(LATEXROOTNAME).tex
	$(foreach texdoc, $(SOURCETEX), perl -pe 's|(\\.*?doxyref){(.*?)}({([1-9]*?)})?|`scripts/$$1 $$2 $$4`|ge' -i $(LATEXBUILDDIR)/$(texdoc);)
	cp $@ $(LATEXBUILDDIR)
	$(DOXYGEN) $@

clean:
ifeq ($(shell test -f $(LATEXBUILDDIR) && echo "YES" || echo "NO"), YES)
	cd $(LATEXBUILDDIR) && $(LATEXMK) -c $(LATEXROOTNAME).tex
endif
	$(RM) -r $(BUILDDIR)
