/*
 * This software was developed at the National Institute of Standards and
 * Technology (NIST) by employees of the Federal Government in the course
 * of their official duties. Pursuant to title 17 Section 105 of the
 * United States Code, this software is not subject to copyright protection
 * and is in the public domain. NIST assumes no responsibility whatsoever for
 * its use by other parties, and makes no guarantees, expressed or implied,
 * about its quality, reliability, or any other characteristic.
 */

#ifndef __PY_LIBBIOMEVAL_H__
#define __PY_LIBBIOMEVAL_H__

#ifdef __APPLE__
#include <AvailabilityMacros.h>
#ifdef MAC_OS_X_VERSION_10_13
#include <Python.h>
#include <structmember.h>
#else /* MAC_OS_X_VERSION_10_13 */
#include <Python/Python.h>
#include <Python/structmember.h>
#endif/* MAC_OS_X_VERSION_10_13 */
#else
#include <Python.h>
#include <structmember.h>
#endif /* __APPLE__ */

#include <string>

#include <be_error_exception.h>

namespace BE = BiometricEvaluation;

namespace PythonBiometricEvaluation
{
	/** Name of the Python module. */
	static const std::string MODULE_NAME = "BiometricEvaluation";
	/** Value symbolizing opening read-only. */
	static const std::string VALUE_READONLY = "READONLY";
	/** Value symbolizing opening with write access. */
	static const std::string VALUE_READWRITE = "READWRITE";

	/*
	 * Helper methods.
	 */

	/**
	 * @brief
	 * Translate BiometricEvaluation exception into a Python exception that
	 * can be used to terminate a method.
	 * 
	 * @param originalException
	 * The exception to convert and throw to Python
	 *
	 * @return
	 * nullptr, which signals to Python to check its exception flag.
	 */
	inline PyObject*
	convertException(
	    const BiometricEvaluation::Error::Exception &originalException)
	{
		try {
			throw originalException;
		} catch (BE::Error::ParameterError &e) {
			PyErr_SetString(PyExc_ValueError, e.what());
			return (nullptr);
		} catch(BE::Error::ObjectDoesNotExist &e) {
			PyErr_SetString(PyExc_ValueError, e.what());
			return (nullptr);
		} catch (BE::Error::MemoryError &e) {
			return (PyErr_NoMemory());
		} catch (BE::Error::Exception &e) {
			PyErr_SetString(PyExc_Exception, e.what());
			return (nullptr);
		}
	}

	/**
	 * @brief
	 * Convenience method to throw and translate a new ParameterError.
	 *
	 * @param parameter
	 * Name of the bad parameter.
	 *
	 * @return
	 * nullptr, which signals to Python to check its exception flag.
	 */
	inline PyObject*
	parameterException(
	    const std::string &parameter = "")
	{
		return (convertException(BE::Error::ParameterError(parameter)));
	}

	/** Convert unowned C-strings passed from Python to C++ strings. */
	std::string
	parseString(
	    const char *pyString);
}

/** Module initialization method. */
PyMODINIT_FUNC
initBiometricEvaluation();

#endif /* __PY_LIBBIOMEVAL_H__ */
