/*
 * This software was developed at the National Institute of Standards and
 * Technology (NIST) by employees of the Federal Government in the course
 * of their official duties. Pursuant to title 17 Section 105 of the
 * United States Code, this software is not subject to copyright protection
 * and is in the public domain. NIST assumes no responsibility whatsoever for
 * its use by other parties, and makes no guarantees, expressed or implied,
 * about its quality, reliability, or any other characteristic.
 */
 
#ifdef __APPLE__
#include <Python/Python.h>
#include <Python/structmember.h>
#else
#include <Python.h>
#include <structmember.h>
#endif

#include "py_recordstore.h"

#include <memory>
#include <be_io_recordstore.h>
#include <be_io_recordstoreiterator.h>

namespace BE = BiometricEvaluation;
namespace PBE = PythonBiometricEvaluation;
namespace PRS = PythonBiometricEvaluation::RecordStore;

/*
 * Iterators.
 */

PyObject*
RSObject_iter(
    PyObject *self)
{
	Py_INCREF(self);
	((RSObject *)self)->cursor = BE::IO::RecordStore::BE_RECSTORE_SEQ_START;
	return (self);
}

PyObject*
RSObject_iternext(
    PyObject *self)
{
	BE::IO::RecordStore::Record record;
	RSObject *rsSelf = (RSObject *)self;

	/* Set the cursor before sequencing in case an exception is thrown */
	bool firstTime =
	    (rsSelf->cursor == BE::IO::RecordStore::BE_RECSTORE_SEQ_START);
	rsSelf->cursor = BE::IO::RecordStore::BE_RECSTORE_SEQ_NEXT;

	try {
		if (firstTime)
			record = rsSelf->rs->sequence(
			    BE::IO::RecordStore::BE_RECSTORE_SEQ_START);
		else
			record = rsSelf->rs->sequence(
			    BE::IO::RecordStore::BE_RECSTORE_SEQ_NEXT);
	} catch (BE::Error::ObjectDoesNotExist) {
		PyErr_SetNone(PyExc_StopIteration);
		return (nullptr);
	} catch (BE::Error::Exception &e) {
		return (PBE::convertException(e));
	}
	
	return (Py_BuildValue("{s:z#}", record.key.c_str(), &(*record.data),
	    record.data.size()));
}

/*
 * CRUD.
 */

PyObject*
RSObject_read(
    RSObject *self,
    PyObject *args,
    PyObject *kwds)
{
	static const char *kwlist[] = {PRS::PARAM_KEY.c_str(), nullptr};
	char *pyKey = nullptr;
	if (!PyArg_ParseTupleAndKeywords(args, kwds, "s",
	    const_cast<char**>(kwlist), &pyKey))
	    	return (PBE::parameterException());
	
	std::string key;
	try {
		key = PBE::parseString(pyKey);
	} catch (BE::Error::Exception &e) {
		return (PBE::convertException(e));
	}

	BE::Memory::uint8Array value;
	try {
		value = self->rs->read(key);
	} catch (BE::Error::Exception &e) {
		return (PBE::convertException(e));
	}

	return (Py_BuildValue("z#", &(*value), value.size()));
}

PyObject*
RSObject_insert(
    RSObject *self,
    PyObject *args,
    PyObject *kwds)
{
	static const char *kwlist[] = {PRS::PARAM_KEY.c_str(),
	    PRS::PARAM_VALUE.c_str(), nullptr};
	char *pyKey = nullptr;
	char *pyValue = nullptr, *cValue = nullptr;
	size_t pyValueSize = 0;
	if (!PyArg_ParseTupleAndKeywords(args, kwds, "zz#",
	    const_cast<char**>(kwlist), &pyKey, &pyValue, &pyValueSize))
	    	return (PBE::parameterException());

	std::string key;
	try {
		key = PBE::parseString(pyKey);
	} catch (BE::Error::Exception &e) {
		return (PBE::convertException(e));
	}

	if (pyValue == nullptr)
		return (PBE::parameterException(PRS::PARAM_VALUE));
	cValue = (char *)malloc(sizeof(char) * pyValueSize);
	if (cValue == nullptr)
		return (PBE::convertException(BE::Error::MemoryError()));
	::memcpy(cValue, pyValue, pyValueSize);

	try {
		self->rs->insert(key, cValue, pyValueSize);
	} catch (BE::Error::Exception &e) {
		free(cValue);
		return (PBE::convertException(e));
	}
	free(cValue);
	
	return (Py_BuildValue(""));
}

PyObject*
RSObject_replace(
    RSObject *self,
    PyObject *args,
    PyObject *kwds)
{
	static const char *kwlist[] = {PRS::PARAM_KEY.c_str(),
	    PRS::PARAM_VALUE.c_str(), nullptr};
	char *pyKey = nullptr;
	char *pyValue = nullptr, *cValue = nullptr;
	size_t pyValueSize = 0;
	if (!PyArg_ParseTupleAndKeywords(args, kwds, "zz#",
	    const_cast<char**>(kwlist), &pyKey, &pyValue, &pyValueSize))
		return (PBE::parameterException());

	std::string key;
	try {
		key = PBE::parseString(pyKey);
	} catch (BE::Error::Exception &e) {
		return (PBE::convertException(e));
	}
	
	if (pyValue == nullptr)
		return (PBE::parameterException(PRS::PARAM_VALUE));
	cValue = (char *)malloc(sizeof(char) * pyValueSize);
	if (cValue == nullptr)
		return (PBE::convertException(BE::Error::MemoryError()));
	::memcpy(cValue, pyValue, pyValueSize);

	try {
		self->rs->replace(key, cValue, pyValueSize);
	} catch (BE::Error::Exception &e) {
		free(cValue);
		return (PBE::convertException(e));
	}
	free(cValue);
	
	return (Py_BuildValue(""));
}

PyObject*
RSObject_remove(
    RSObject *self,
    PyObject *args,
    PyObject *kwds)
{
	static const char *kwlist[] = {PRS::PARAM_KEY.c_str(), nullptr};
	char *pyKey = nullptr;
	if (!PyArg_ParseTupleAndKeywords(args, kwds, "s",
	    const_cast<char**>(kwlist), &pyKey))
	    	return (PBE::parameterException());
	
	std::string key;
	try {
		key = PBE::parseString(pyKey);
	} catch (BE::Error::Exception &e) {
		return (PBE::convertException(e));
	}
	
	/*
	 * Remove value.
	 */
	try {
		self->rs->remove(key);
	} catch (BE::Error::Exception &e) {
		return (PBE::convertException(e));
	}
	
	return (Py_BuildValue(""));
}

/*
 * Information about the RecordStore's contents.
 */

PyObject*
RSObject_length(
    RSObject *self,
    PyObject *args,
    PyObject *kwds)
{
	static const char *kwlist[] = {PRS::PARAM_KEY.c_str(), nullptr};
	char *pyKey = nullptr;
	if (!PyArg_ParseTupleAndKeywords(args, kwds, "s",
	    const_cast<char**>(kwlist), &pyKey))
	    	return (PBE::parameterException());
	
	std::string key;
	try {
		key = PBE::parseString(pyKey);
	} catch (BE::Error::Exception &e) {
		return (PBE::convertException(e));
	}

	try {
		return (Py_BuildValue("K", self->rs->length(key)));
	} catch (BE::Error::Exception &e) {
		return (PBE::convertException(e));
	}
}

/*
 * RecordStore maintenance.
 */

PyObject*
RSObject_changeDescription(
    RSObject *self,
    PyObject *args,
    PyObject *kwds)
{
	static const char *kwlist[] = {PRS::PARAM_DESCRIPTION.c_str(), nullptr};
	char *pyDesc = nullptr;
	if (!PyArg_ParseTupleAndKeywords(args, kwds, "z",
	    const_cast<char**>(kwlist), &pyDesc))
	    	return (PBE::parameterException());
	
	std::string desc;
	try {
		desc = PBE::parseString(pyDesc);
	} catch (BE::Error::Exception &e) {
		return (PBE::convertException(e));
	}
	
	try {
		self->rs->changeDescription(desc);
	} catch (BE::Error::Exception &e) {
		return (PBE::convertException(e));
	}
	
	return (Py_BuildValue(""));
}

PyObject*
RSObject_sync(
    RSObject *self)
{
	try {
		self->rs->sync();
		return (Py_BuildValue(""));
	} catch (BE::Error::Exception &e) {
		return (PBE::convertException(e));
	}
}

PyObject*
RSObject_flush(
    RSObject *self,
    PyObject *args,
    PyObject *kwds)
{
	static const char *kwlist[] = {PRS::PARAM_KEY.c_str(), nullptr};
	char *pyKey = nullptr;
	if (!PyArg_ParseTupleAndKeywords(args, kwds, "s",
	    const_cast<char**>(kwlist), &pyKey))
	    	return (PBE::parameterException());
	
	std::string key;
	try {
		key = PBE::parseString(pyKey);
	} catch (BE::Error::Exception &e) {
		return (PBE::convertException(e));
	}

	try {
		self->rs->flush(key);
	} catch (BE::Error::Exception &e) {
		return (PBE::convertException(e));
	}
	
	return (Py_BuildValue(""));
}

/*
 * RecordStore information.
 */

PyObject*
RSObject_containsKey(
    RSObject *self,
    PyObject *args,
    PyObject *kwds)
{
	static const char *kwlist[] = {PRS::PARAM_KEY.c_str(), nullptr};
	char *pyKey = nullptr;
	if (!PyArg_ParseTupleAndKeywords(args, kwds, "s",
	    const_cast<char**>(kwlist), &pyKey))
	    	return (PBE::parameterException());

	std::string key;
	try {
		key = PBE::parseString(pyKey);
	} catch (BE::Error::Exception &e) {
		return (PBE::convertException(e));
	}

	if (self->rs->containsKey(key))
		Py_RETURN_TRUE;
	else 
		Py_RETURN_FALSE;
}

PyObject*
RSObject_description(
    RSObject *self)
{
	try {
		return (Py_BuildValue("z", self->rs->getDescription().c_str()));
	} catch (BE::Error::Exception &e) {
		return (PBE::convertException(e));
	}
}

PyObject*
RSObject_count(
    RSObject *self)
{
	try {
		return (Py_BuildValue("K", self->rs->getCount()));
	} catch (BE::Error::Exception &e) {
		return (PBE::convertException(e));
	}
}

PyObject*
RSObject_spaceUsed(
    RSObject *self)
{
	try {
		return (Py_BuildValue("K", self->rs->getSpaceUsed()));
	} catch (BE::Error::Exception &e) {
		return (PBE::convertException(e));
	}
}

/*
 * Python RecordStore object lifecycle.
 */

void
RSObject_dealloc(
    PyObject *self)
{
	((RSObject*)self)->rs.reset();
	self->ob_type->tp_free(self);
}

PyObject*
RSObject_new(
    PyTypeObject *type,
    PyObject *args,
    PyObject *kwds)
{
	RSObject *self;

	self = (RSObject *)type->tp_alloc(type, 0);
	if (self == nullptr)
		return (PBE::convertException(BE::Error::MemoryError()));

	self->cursor = BE::IO::RecordStore::BE_RECSTORE_SEQ_START;

	return ((PyObject *)self);
}

int
RSObject_init(
    RSObject *self,
    PyObject *args,
    PyObject *kwds)
{
	static const char *kwlist[] = {PRS::PARAM_PATHNAME.c_str(),
	    PRS::PARAM_MODE.c_str(), PRS::PARAM_RSTYPE.c_str(),
	    PRS::PARAM_DESCRIPTION.c_str(), nullptr};
	char *pyPathname = nullptr;
	char *pyDesc = nullptr;
	int8_t mode = -1, type = -1;
	if (!PyArg_ParseTupleAndKeywords(args, kwds, "z|hhz",
	    const_cast<char**>(kwlist), &pyPathname, &mode, &type, &pyDesc)) {
    		PBE::parameterException();
	    	return (-1);
	}

	/*
	 * Check values.
	 */
	if (pyPathname == nullptr) {
		PBE::parameterException(PRS::PARAM_PATHNAME);
		return (-1);
	}
	/* If mode was omitted */
	if (mode == -1)
		mode = static_cast<std::underlying_type<BE::IO::Mode>::type>(
		    BE::IO::Mode::ReadOnly);
	else if ((mode != static_cast<std::underlying_type<BE::IO::Mode>::type>(
	    BE::IO::Mode::ReadOnly)) && (mode !=
	    static_cast<std::underlying_type<BE::IO::Mode>::type>(
	    BE::IO::Mode::ReadWrite))) {
		PBE::parameterException(PRS::PARAM_MODE);
		return (-1);
	}
	/* If type is not omitted, require a description */
	if (type != -1) {
		if (pyDesc == nullptr) {
			PBE::parameterException(PRS::PARAM_DESCRIPTION);
			return (-1);
		}
	}

	std::string pathname;
	try {
		pathname = PBE::parseString(pyPathname);
	} catch (BE::Error::Exception &e) {
		PBE::convertException(e);
		return (-1);
	}
	
	std::string desc;
	if (type != -1) {
		try {
			desc = PBE::parseString(pyDesc);
		} catch (BE::Error::Exception &e) {
			PBE::convertException(e);
			return (-1);
		}
	}
	
	/*
	 * Open/create the RecordStore in C++.
	 */
	try {
		/* Open if type was omitted */
		if (type == -1)
			self->rs = BE::IO::RecordStore::openRecordStore(
			    pathname, static_cast<BE::IO::Mode>(mode));
		else {
			auto kind = to_enum<BE::IO::RecordStore::Kind>(type);
			self->rs = BE::IO::RecordStore::createRecordStore(
			    pathname, desc, kind);
		}
	} catch (BE::Error::Exception &e) {
		PBE::convertException(e);
		return (-1);
	}
	
	if (self->rs == nullptr) {
		PBE::convertException(BE::Error::MemoryError());
		return (-1);
	}
		
	return (0);
}

PyObject*
removeRecordStore(
    RSObject *self,
    PyObject *args,
    PyObject *kwds)
{
	static const char *kwlist[] = {PRS::PARAM_PATHNAME.c_str(), nullptr};
	char *pyPathname = nullptr;
	if (!PyArg_ParseTupleAndKeywords(args, kwds, "s",
	    const_cast<char**>(kwlist), &pyPathname)) {
    		return (PBE::parameterException());
	}

	if (pyPathname == nullptr) {
		return (PBE::parameterException(PRS::PARAM_PATHNAME));
	}

	std::string pathname;
	try {
		pathname = PBE::parseString(pyPathname);
	} catch (BE::Error::Exception &e) {
		return (PBE::convertException(e));
	}

	try {
		BE::IO::RecordStore::removeRecordStore(pathname);
	} catch (BE::Error::Exception &e) {
		return (PBE::convertException(e));
	}

	return (Py_BuildValue(""));
}
