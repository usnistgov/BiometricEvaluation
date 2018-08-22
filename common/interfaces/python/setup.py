#!/usr/bin/env python
# This software was developed at the National Institute of Standards and
# Technology (NIST) by employees of the Federal Government in the course
# of their official duties. Pursuant to title 17 Section 105 of the
# United States Code, this software is not subject to copyright protection
# and is in the public domain. NIST assumes no responsibility whatsoever for
# its use by other parties, and makes no guarantees, expressed or implied,
# about its quality, reliability, or any other characteristic.
 
import platform
from distutils.core import setup, Extension

module = Extension('BiometricEvaluation', sources = ['py_libbiomeval.cpp', 'py_recordstore.cpp'])
module.include_dirs = ['/usr/nistlocal/biomeval/include']
module.library_dirs = ['/usr/nistlocal/biomeval/lib64']
module.libraries = ['biomeval']
module.extra_compile_args = ['-std=c++11']
module.extra_link_args = ['-Wl,-rpath,/usr/nistlocal/biomeval/lib64']

if platform.mac_ver()[0] != "":
	module.extra_compile_args += ['-Wno-error=unused-command-line-argument']

module.language = ['c++']

version_info = dict()
with open("../../VERSION") as file:
	for line in file:
		if line.startswith('#') or line.startswith('\n'):
			continue
		key, value = line.split('=')
		version_info[key.strip()] = value.strip()

setup(name="BiometricEvaluation", version="{0}.{1}".format(version_info["MAJOR_VERSION"], version_info["MINOR_VERSION"]), ext_modules = [module])

