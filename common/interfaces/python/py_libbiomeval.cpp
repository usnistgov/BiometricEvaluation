/*
 * This software was developed at the National Institute of Standards and
 * Technology (NIST) by employees of the Federal Government in the course
 * of their official duties. Pursuant to title 17 Section 105 of the
 * United States Code, this software is not subject to copyright protection
 * and is in the public domain. NIST assumes no responsibility whatsoever for
 * its use by other parties, and makes no guarantees, expressed or implied,
 * about its quality, reliability, or any other characteristic.
 */

#include "py_recordstore.h"
#include "py_libbiomeval.h"

namespace PBE = PythonBiometricEvaluation;

/** Public methods of the BiometricEvaluation module. */
static PyMethodDef biomeval_methods[] =
{
	{nullptr, nullptr, 0, nullptr}
};

PyMODINIT_FUNC
initBiometricEvaluation()
{
	PyObject *module;

	RSType.tp_new = PyType_GenericNew;
	if (PyType_Ready(&RSType) < 0)
		return;
	
	module = Py_InitModule3(PBE::MODULE_NAME.c_str(), biomeval_methods,
	    "NIST Image Group Biometric Evaluation");
	
	Py_INCREF(&RSType);
	PyModule_AddObject(module, PRS::OBJECT_NAME.c_str(),
	    (PyObject*)&RSType);

	/* 
	 * Module "constants".
	 *
	 * NOTE:
	 * Python does not have the concept of constants, and especially not
	 * at the type level. Probably should rethink this.
	 */

	/* Open Mode constants */
	PyModule_AddIntConstant(module, PBE::VALUE_READONLY.c_str(),
	    static_cast<std::underlying_type<BE::IO::Mode>::type>(
	    BE::IO::Mode::ReadOnly));
	PyModule_AddIntConstant(module, PBE::VALUE_READWRITE.c_str(),
	    static_cast<std::underlying_type<BE::IO::Mode>::type>(
	    BE::IO::Mode::ReadWrite));

	/* RecordStore Type constants */
	PyModule_AddIntConstant(module, (PBE::RecordStore::OBJECT_NAME + "_" +
	    to_string(BE::IO::RecordStore::Kind::BerkeleyDB)).c_str(),
	    to_int_type(BE::IO::RecordStore::Kind::BerkeleyDB));
	PyModule_AddIntConstant(module, (PBE::RecordStore::OBJECT_NAME + "_" +
	    to_string(BE::IO::RecordStore::Kind::Archive)).c_str(),
	    to_int_type(BE::IO::RecordStore::Kind::Archive));
	PyModule_AddIntConstant(module, (PBE::RecordStore::OBJECT_NAME + "_" +
	    to_string(BE::IO::RecordStore::Kind::File)).c_str(),
	    to_int_type(BE::IO::RecordStore::Kind::File));
	PyModule_AddIntConstant(module, (PBE::RecordStore::OBJECT_NAME + "_" +
	    to_string(BE::IO::RecordStore::Kind::SQLite)).c_str(),
	    to_int_type(BE::IO::RecordStore::Kind::SQLite));
	PyModule_AddIntConstant(module, (PBE::RecordStore::OBJECT_NAME + "_" +
	    to_string(BE::IO::RecordStore::Kind::Compressed)).c_str(),
	    to_int_type(BE::IO::RecordStore::Kind::Compressed));
	PyModule_AddIntConstant(module,  (PBE::RecordStore::OBJECT_NAME + "_" +
	    PRS::PARAM_RSTYPE_VALUE_DEFAULT).c_str(),
	    to_int_type(BE::IO::RecordStore::Kind::Default));
}

std::string
PythonBiometricEvaluation::parseString(
    const char *pyString)
{
	if (pyString == nullptr)
		throw BE::Error::ParameterError();

	char *cString = nullptr;
	size_t strSize = strlen(pyString) + 1;

	cString = new(std::nothrow) char[strSize];
	if (cString == nullptr)
		throw BE::Error::MemoryError();

	::strncpy(cString, pyString, strSize);
	std::string cppString(cString);

	delete[] cString;
	return (cppString);
}
