# This software was developed at the National Institute of Standards and
# Technology (NIST) by employees of the Federal Government in the course
# of their official duties. Pursuant to title 17 Section 105 of the
# United States Code, this software is not subject to copyright protection
# and is in the public domain. NIST assumes no responsibility whatsoever for
# its use by other parties, and makes no guarantees, expressed or implied,
# about its quality, reliability, or any other characteristic.

#
# To create a Makefile for a LaTeX document:
#
#     - include doccommon/common.mk
#
#     - Define the following make variables:
#	- ROOTNAME: The document 'core' files (main .tex, .bib, etc.)
#	- WRAPPER: File that wraps ROOTNAME.tex
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
# To add doxygen documentation:
#
#     - If doxygen is to be run and the doxygen configuration file is not 
#	$(ROOTNAME).dox, define DOXYCONFIG and set GENERATEDOXYGEN to YES
#
# Good luck.
#

#
# Override executables
#
#
LATEX = $(shell which pdflatex)
MAKEINDEX = $(shell which makeindex)
MAKEBIB = $(shell which bibtex)
DOXYGEN = $(shell which doxygen)
CP = cp -pf
LNS = ln -sf

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
# The PDF file to be dropped in the top directory.
#
PDFMAIN = $(WRAPPER).pdf

#
# Setup the build directory structure and file names that will be used
# in that directory.
#
LATEXROOTNAME = $(WRAPPER)
BUILDDIR = build
LATEXBUILDDIR = $(BUILDDIR)/latex
HTMLBUILDDIR = $(BUILDDIR)/html
LATEXPDF = $(LATEXROOTNAME).pdf
LATEXBIB = $(LATEXROOTNAME).bib
LATEXMAIN = $(LATEXROOTNAME).tex

#
# If the common glossary is wanted, we need to tell later on.
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

thedoc: $(PDFMAIN)
# Error if main Makefile does not define a default goal
ifeq ($(.DEFAULT_GOAL), )
	$(error DEFAULT_GOAL is not set)
endif

#
# Check whether the final PDF file is writable; the build will still
# occur if not, however.
#
$(PDFMAIN): $(LATEXBUILDDIR)/$(LATEXPDF)
ifneq ($(PDFMAINWRITABLE),YES)
	$(error $(PDFMAIN) is not writable)
else
	$(CP) $(LATEXBUILDDIR)/$(LATEXPDF) $(PDFMAIN)
endif

#
# Build relies on doxygen config, if present
#
ifeq ($(GENERATEDOXYGEN), YES)
$(LATEXBUILDDIR)/$(LATEXPDF): $(SOURCETEX) $(ROOTNAME).tex $(WRAPPER).tex $(DOXYCONFIG)
else
$(LATEXBUILDDIR)/$(LATEXPDF): $(SOURCETEX) $(ROOTNAME).tex $(WRAPPER).tex
endif
#
# Make directory structure
#
	mkdir -p $(LATEXBUILDDIR)
	mkdir -p $(HTMLBUILDDIR)
#
# Combine project-level bibliography files with the common references
#
	cat $(SOURCEBIB) $(LOCALINC)/shared.bib > $(LATEXBUILDDIR)/$(LATEXBIB)
ifneq ($(SOURCEBIB), $(LATEXBIB))
	$(LNS) $(LATEXBIB) $(LATEXBUILDDIR)/$(SOURCEBIB)
endif
	$(CP) $(SOURCETEX) $(LATEXBUILDDIR)
#
# Are any common TeX files included?
#
ifdef COMMONTEX
	$(CP) $(COMMONTEX) $(LATEXBUILDDIR)
endif
#
# Copy any assets into the build directory
#
ifdef COMMONASSETS
	$(CP) $(COMMONASSETS) $(LATEXBUILDDIR)
endif
#
# Add rule to generate glossary, if present
#
ifeq ($(GENERATEGLOSSARY), YES)
	$(LNS) $(LATEXBUILDDIR)/$(GLOSSARY) $(LATEXBUILDDIR)/$(basename $(GLOSSARY)).glo
	$(CP) $(LOCALINC)/latexmkrc $(LATEXBUILDDIR)
endif
	$(CP) $(WRAPPER).tex $(LATEXBUILDDIR)/$(LATEXROOTNAME).tex
#
# When generating Doxygen content, we need to first modify the source LaTeX
# files to have linked references to doxygen content. The run Doxygen to
# create the class and other documents.
#
ifeq ($(GENERATEDOXYGEN), YES)
	chmod +w $(LATEXBUILDDIR)/*.tex
	$(foreach texdoc, $(SOURCETEX), perl -pe 's|(\\.*?doxyref){(.*?)}({([1-9]*?)})?|`scripts/$$1 $$2 $$4`|ge' -i $(LATEXBUILDDIR)/$(texdoc);)
	$(CP) $(DOXYCONFIG) $(LATEXBUILDDIR)
	$(DOXYGEN) $(DOXYCONFIG)
endif

#
# Build the document, including index, bibliography.
#
	cd $(LATEXBUILDDIR) && $(LATEX) $(LATEXROOTNAME)
	cd $(LATEXBUILDDIR) && $(MAKEINDEX) $(LATEXROOTNAME)
	cd $(LATEXBUILDDIR) && $(MAKEBIB) $(LATEXROOTNAME)
	cd $(LATEXBUILDDIR) && $(LATEX) $(LATEXROOTNAME)
	cd $(LATEXBUILDDIR) && $(LATEX) $(LATEXROOTNAME)

clean:
	$(RM) -r $(BUILDDIR)
