/*
 * This software was developed at the National Institute of Standards and
 * Technology (NIST) by employees of the Federal Government in the course
 * of their official duties. Pursuant to title 17 Section 105 of the
 * United States Code, this software is not subject to copyright protection
 * and is in the public domain. NIST assumes no responsibility whatsoever for
 * its use by other parties, and makes no guarantees, expressed or implied,
 * about its quality, reliability, or any other characteristic.
 */

#ifndef __PY_RECORDSTORE_H__
#define __PY_RECORDSTORE_H__

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

/* 
 * structmember.h #defines READONLY
 */
#ifdef READONLY
#undef READONLY
#endif

#include "py_libbiomeval.h"

#include <memory>

#include <be_io_recordstore.h>

namespace BE = BiometricEvaluation;
namespace PBE = PythonBiometricEvaluation;

namespace PythonBiometricEvaluation
{
	namespace RecordStore
	{
		/** Name of the RecordStore  object defined in the module. */
		static const std::string OBJECT_NAME = "RecordStore";
		/** Fully-qualified name of Python object. */
		static const std::string DOTTED_NAME = PBE::MODULE_NAME + "." +
		    OBJECT_NAME;
		/** Parameter used for passing keys. */
		static const std::string PARAM_KEY = "key";
		/** Parameter used for passing values. */
		static const std::string PARAM_VALUE = "value";
		/** Parameter used for passing pathnames. */
		static const std::string PARAM_PATHNAME = "path";
		/** Parameter used for passing RecordStore open modes. */
		static const std::string PARAM_MODE = "mode";
		/** Parameter used for passing RecordStore types. */
		static const std::string PARAM_RSTYPE = "rstype";
		/** Value for PARAM_RSTYPE to use the default RS type. */
		static const std::string PARAM_RSTYPE_VALUE_DEFAULT = "Default";
		/** Parameter used for passing descriptions of RecordStores. */
		static const std::string PARAM_DESCRIPTION = "description";
	}
}
namespace PRS = PythonBiometricEvaluation::RecordStore;


/** Representation of a C++ RecordStore in Python. */
typedef struct {
	PyObject_HEAD /* No semicolon. Really. */
	std::shared_ptr<BE::IO::RecordStore> rs;
	int cursor;
} RSObject;

/*
 * Iterators.
 */

/** Iterator. */
PyObject*
RSObject_iter(
    PyObject *self);

/** Iterate to next item. */
PyObject*
RSObject_iternext(
    PyObject *self);

/*
 * CRUD.
 */

/** Insert a value into the RecordStore. */
PyObject*
RSObject_insert(
    RSObject *self,
    PyObject *args,
    PyObject *kwds); 

/** Read a value from the RecordStore. */
PyObject*
RSObject_read(
    RSObject *self,
    PyObject *args,
    PyObject *kwds);

/** Remove a value from the RecordStore. */
PyObject*
RSObject_remove(
    RSObject *self, 
    PyObject *args,
    PyObject *kwds);

/** Replace a value from the RecordStore with a new value. */
PyObject*
RSObject_replace(
    RSObject *self,
    PyObject *args,
    PyObject *kwds);

/*
 * Information about the RecordStore's contents.
 */

/** Obtain the length of the value of a key. */
PyObject*
RSObject_length(
    RSObject *self,
    PyObject *args,
    PyObject *kwds);

/*
 * RecordStore maintenance.
 */
 
/** Synchronize the entire RecordStore to persistent storage. */
PyObject*
RSObject_sync(
    RSObject *self);

/** Change the user-supplied description of the RecordStore */
PyObject*
RSObject_changeDescription(
    RSObject *self,
    PyObject *args,
    PyObject *kwds);
    
/** Flush a specific record's data to storage. */
PyObject* RSObject_flush(
    RSObject *self,
    PyObject *args,
    PyObject *kwds);

/*
 * RecordStore information.
 */

/** Obtain the user-supplied description for the RecordStore. */
PyObject*
RSObject_description(
    RSObject *self);
    
/** Obtain the number of records in the RecordStore. */
PyObject*
RSObject_count(
    RSObject *self);

/** Obtain the size of the RecordStore on disk. */
PyObject*
RSObject_spaceUsed(
    RSObject *self);
    
/** Obtain whether or not a particular key is in the RecordStore. */
PyObject*
RSObject_containsKey(
    RSObject *self,
    PyObject *args, 
    PyObject *kwds);

/*
 * Python RecordStore object lifecycle.
 */

/** __init__ */
int
RSObject_init(
    RSObject *self,
    PyObject *args,
    PyObject *kwds);

/** __dealloc__ */    
void
RSObject_dealloc(
    PyObject *self);

/** __new__ */
PyObject*
RSObject_new(
    PyTypeObject *type,
    PyObject *args,
    PyObject *kwds);

/*
 * Static methods
 */

/** Delete all persistant data associated with the store. */
PyObject*
removeRecordStore(
    RSObject *self,
    PyObject *args,
    PyObject *kwds);

/** Public methods of the recordstore.RecordStore object. */
static PyMethodDef RSObject_methods[] = 
{
	/* CRUD */
	{"read", (PyCFunction)RSObject_read, METH_VARARGS | METH_KEYWORDS,
	    "Returns the value for the specified key"},
	{"insert", (PyCFunction)RSObject_insert, METH_VARARGS | METH_KEYWORDS,
	    "Insert a record into the RecordStore"},
	{"remove", (PyCFunction)RSObject_remove, METH_VARARGS | METH_KEYWORDS,
	    "Remove a record from the RecordStore"},
	{"replace", (PyCFunction)RSObject_replace, METH_VARARGS | METH_KEYWORDS,
	    "Replace a complete record in the RecordStore"},

	/* RecordStore information */
	{"description", (PyCFunction)RSObject_description, METH_NOARGS,
	    "Obtain a textual description of the RecordStore."},
	{"count", (PyCFunction)RSObject_count, METH_NOARGS,
	    "Number of objects in the RecordStore"},
    	{"contains_key", (PyCFunction)RSObject_containsKey,
	    METH_VARARGS | METH_KEYWORDS,
	    "Determines whether or not the RecordStore contains a record\n"
	    "with the specified key"},
   	{"space_used", (PyCFunction)RSObject_spaceUsed, METH_NOARGS,
	    "Obtain real storage utilization."},

	/* RecordStore maintaince */
	{"change_description", (PyCFunction)RSObject_changeDescription,
	    METH_VARARGS | METH_KEYWORDS, "Change the description of the "
	    "RecordStore"},
	{"sync", (PyCFunction)RSObject_sync, METH_NOARGS, "Synchronize the "
	    "entire RecordStore to persistent storage."},
	{"flush", (PyCFunction)RSObject_flush, METH_VARARGS | METH_KEYWORDS,
	    "Commit a record's data to storage"},

	/* Record information */
	{"length", (PyCFunction)RSObject_length, METH_VARARGS | METH_KEYWORDS,
	    "Obtain the length of a record"},

	{"delete", (PyCFunction)removeRecordStore, METH_VARARGS | 
	    METH_KEYWORDS | METH_STATIC, "Delete all persistant data "
	    "associated with a RecordStore."},

	/* The End. */
	{nullptr, nullptr, 0, nullptr}
};

/** Public members of a recordstore.RecordStore object. */
static PyMemberDef RSObject_members[] = 
{
	{nullptr, 0, 0, 0, nullptr}
};

/** Public documentation of the recordstore.RecordStore object. */
static const std::string RSObjectDocumentation =
    PRS::OBJECT_NAME + "(" + PRS::PARAM_PATHNAME +
    " [, " + PRS::PARAM_MODE + ", " + PRS::PARAM_RSTYPE + ", " +
    PRS::PARAM_DESCRIPTION + "])\n\nTo open:\nrs = " + PRS::OBJECT_NAME +
    "(\"/path/to/rs\", " + PRS::PARAM_MODE + " = " + PBE::MODULE_NAME +
    "." + PBE::VALUE_READWRITE + "])\n\nTo create:\nrs = " +
    PRS::OBJECT_NAME + "(\"/path/to/rs\",\n    " + PRS::PARAM_RSTYPE +
    " = " + PBE::MODULE_NAME + "." + PRS::PARAM_RSTYPE_VALUE_DEFAULT +
    ",\n    " + PRS::PARAM_DESCRIPTION + " = \"A new " +  PRS::OBJECT_NAME +
    "\")";

/** Python definition of the recordstore.RecordStore type. */
static PyTypeObject RSType =
{
	PyObject_HEAD_INIT(NULL)
	0,						/* ob_size */
	PRS::DOTTED_NAME.c_str(),			/* tp_name */
	sizeof(RSObject),				/* tp_basicsize */
	0,						/* tp_itemsize */
	RSObject_dealloc,				/* tp_dealloc */
	0,						/* tp_print */
	0,						/* tp_getattr */
	0,						/* tp_setattr */
	0,						/* tp_compare */
	0,						/* tp_repr */
	0,						/* tp_as_number */
	0,						/* tp_as_sequence */
	0,						/* tp_as_mapping */
	0,						/* tp_hash */
	0,						/* tp_call */
	0,						/* tp_str */
	0,						/* tp_getattro */
	0,						/* tp_setattro */
	0,						/* tp_as_buffer */
	Py_TPFLAGS_DEFAULT | Py_TPFLAGS_HAVE_ITER,	/* tp_flags */
	RSObjectDocumentation.c_str(),			/* tp_doc */
	0,						/* tp_traverse */
	0,						/* tp_clear */
	0,						/* tp_richcompare */
	0,						/* tp_weaklistoffset */
	RSObject_iter,					/* tp_iter */
	RSObject_iternext,				/* tp_iternext */
	RSObject_methods,				/* tp_methods */
	RSObject_members,				/* tp_members */
	0,						/* tp_getset */
	0,						/* tp_base */
	0,						/* tp_dict */
	0,						/* tp_descr_get */
	0,						/* tp_descr_set */
	0,						/* tp_dictoffset */
	(initproc)RSObject_init,			/* tp_init */
	0,						/* tp_alloc */
	RSObject_new					/* tp_new */
};

#endif /* __PY_RECORDSTORE_H__ */
