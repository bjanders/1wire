/* Copyright (C) Bjorn Andersson <bjorn@iki.fi> */

#include <Python.h>

#include "ds2490.h"


/*********************************************
 * OwUsb
 *********************************************/


typedef struct {
	PyObject_HEAD
	owusb_device_t *dev;
} OwUsbObject;

typedef struct  {
	PyObject_HEAD
	owusb_device_t *dev;
	int init;
	int cmd;
} OwDevIter;

staticforward PyTypeObject OwDevIterType;

#if 0
static PyObject *
OwUsb_new(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
	OwUsbObject *self;
	self =  (OwUsbObject *)type->tp_alloc(type, 0);
	return (PyObject *)self;
}
#endif

static int
OwUsbObject_init(OwUsbObject *self, PyObject *args, PyObject *kwds)
{
	int devnum;

	if (!PyArg_ParseTuple(args, "i", &devnum)) {
		return -1;
	}
	if (devnum >= owusb_dev_count) {
		return -1;
	}
	self->dev = &owusb_devs[devnum];	
	return 0;
}

static PyObject *
ow_search(OwUsbObject *self, PyObject *args)
{
	PyObject *l;
	PyObject *s;
	uint8_t owdevs[256][8];
	int devcount;
	int len;
	int i;
	int cmd = 0xf0;

	if (!PyArg_ParseTuple(args, "|i", &cmd)) {
		return NULL;
	}

	len = owusb_search(self->dev, cmd, (uint8_t *)owdevs, 256 * 8);
	devcount = len / 8;

	l = PyList_New(0);
	for (i = 0; i < devcount; i++) {
		s = PyString_FromStringAndSize((char *)owdevs[i], 8);
		PyList_Append(l, s);
		Py_DECREF(s);
	}
	return l;
	
}

static PyObject *
ow_search_first(OwUsbObject *self, PyObject *args)
{
	int cmd = 0xf0;
	int r;
	uint8_t owdev[8];

	if (!PyArg_ParseTuple(args, "|i", &cmd)) {
		return NULL;
	}
	r = owusb_search_first(self->dev, cmd, owdev);
	if (r != 1) {
		Py_RETURN_NONE;
	}
	return PyString_FromStringAndSize((char *)owdev, 8);
}


static PyObject *
ow_search_next(OwUsbObject *self)
{
	int r;
	uint8_t owdev[8];

	r = owusb_search_next(self->dev, owdev);
	if (r != 1) {
		Py_RETURN_NONE;
	}
	return PyString_FromStringAndSize((char *)owdev, 8);
}

static PyObject *
ow_wait_for_presence(OwUsbObject *self)
{
	owusb_wait_for_presence(self->dev);
	Py_INCREF(Py_None);
	return Py_None;
}

static PyObject *
ow_presence_detect(OwUsbObject *self)
{
	if (owusb_presence_detect(self->dev)) {
		Py_RETURN_TRUE;
	} else {
		Py_RETURN_FALSE;
	}
}

static PyObject *
ow_read_bit(OwUsbObject *self)
{
	int i;
	i = owusb_read_bit(self->dev);
	return Py_BuildValue("i", i);
}

static PyObject *
ow_cmd(OwUsbObject *self, PyObject *args) 
{
	const char *addr;
	int addrlen;
	unsigned char cmd;
	uint8_t outbuf[64];
	int outlen;
	int result;

	if (!PyArg_ParseTuple(args, "s#Bi", &addr, &addrlen, &cmd, &outlen)) {
		return NULL;
	}
	if (addrlen != 8) {
		PyErr_SetString(PyExc_ValueError, "Address must be 8 bytes long");
		return NULL;
	}
	if (outlen > 64) {
		PyErr_SetString(PyExc_ValueError, "Output cannot be longer than 64 bytes");
		return NULL;
	}
	result = owusb_cmd(self->dev, addr, cmd, outbuf, outlen);
	return Py_BuildValue("s#", outbuf, result);
}

static PyObject *
ow_reset(OwUsbObject *self)
{
	uint16_t result;
	
	result = owusb_reset(self->dev);
	return Py_BuildValue("H", result);
}

static PyObject *
ow_write_byte(OwUsbObject *self, PyObject *args)
{
	unsigned char byte;
	int result;
	

	if (!PyArg_ParseTuple(args, "B", &byte)) {
		return NULL;
	}
	result = owusb_write_byte(self->dev, byte);
	return Py_BuildValue("i", result);
}

static PyObject *
ow_block_io(OwUsbObject *self, PyObject *args, PyObject *kwds)
{
	uint8_t readbuf[128];
	uint8_t *writebuf;
	int writebuflen;
	int readbuflen = 0;
	int reset = 0;
	int spu = 0;
	
	/*static char *kwlist[] = { "reset", "spu", NULL };*/
	static char *kwlist[] = { "cmd", "readlen", "reset", "spu", NULL };

	if (!PyArg_ParseTupleAndKeywords(args, kwds, "s#|iii", kwlist, &writebuf, &writebuflen, &readbuflen, &reset, &spu)) {
		return NULL;
	}
	owusb_block_io(self->dev, writebuf, writebuflen, readbuf, readbuflen, reset, spu);
	
	return Py_BuildValue("s#", readbuf, readbuflen);
}

static PyObject *
ow_searchiter(OwUsbObject *self, PyObject *args, PyObject *kwds)
{
	OwDevIter *o;
	PyObject *i;
	uint8_t cmd = 0xf0;

	if (!PyArg_ParseTuple(args, "|c", &cmd)) {
		return NULL;
	}

	o = (OwDevIter *)PyObject_New(OwDevIter, &OwDevIterType);
	o->dev = self->dev;
	o->init = 0;
	o->cmd = cmd;
	i = PyCallIter_New((PyObject *)o, Py_None);
	Py_DECREF(o);
	return i;
}

static PyMethodDef OwUsbObject_methods[] = {
	{ "search", (PyCFunction)ow_search, METH_VARARGS, "Find 1-wire devices" },
	{ "wait_for_presence", (PyCFunction)ow_wait_for_presence, METH_NOARGS, "Wait until a device is present" },
	{ "presence_detect", (PyCFunction)ow_presence_detect, METH_NOARGS, "" },
	{ "write_byte", (PyCFunction)ow_write_byte, METH_VARARGS, "Write a single byte" },
	{ "read_bit", (PyCFunction)ow_read_bit, METH_NOARGS, "Read a single bit" },
	{ "cmd", (PyCFunction)ow_cmd, METH_VARARGS, "Send a command" },
	{ "reset", (PyCFunction)ow_reset, METH_NOARGS, "Send a reset pulse" },
	{ "block_io", (PyCFunction)ow_block_io,  METH_KEYWORDS, "Block IO" },
	{ "search_first", (PyCFunction)ow_search_first, METH_VARARGS, "Find first 1-wire device"},
	{ "search_next", (PyCFunction)ow_search_next, METH_NOARGS, "Find next 1-wire device"},
	{ "searchiter", (PyCFunction)ow_searchiter, METH_VARARGS },
	{NULL}
};


static PyTypeObject OwUsbType = {
    PyObject_HEAD_INIT(NULL)
    0,                         /*ob_size*/
    "owusb.OwUsb",                /*tp_name*/
    sizeof(OwUsbObject),       /*tp_basicsize*/
    0,                         /*tp_itemsize*/
    0,                         /*tp_dealloc*/
    0,                         /*tp_print*/
    0,                         /*tp_getattr*/
    0,                         /*tp_setattr*/
    0,                         /*tp_compare*/
    0,                         /*tp_repr*/
    0,                         /*tp_as_number*/
    0,                         /*tp_as_sequence*/
    0,                         /*tp_as_mapping*/
    0,                         /*tp_hash */
    0,                         /*tp_call*/
    0,                         /*tp_str*/
    0,                         /*tp_getattro*/
    0,                         /*tp_setattro*/
    0,                         /*tp_as_buffer*/
    Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,        /*tp_flags*/
    "1-Wire USB interface",    /* tp_doc */
    0,		               /* tp_traverse */
    0,		               /* tp_clear */
    0,		               /* tp_richcompare */
    0,		               /* tp_weaklistoffset */
    0,		               /* tp_iter */
    0,		               /* tp_iternext */
    OwUsbObject_methods,       /* tp_methods */
    0,                         /* tp_members */
    0,                         /* tp_getset */
    0,                         /* tp_base */
    0,                         /* tp_dict */
    0,                         /* tp_descr_get */
    0,                         /* tp_descr_set */
    0,                         /* tp_dictoffset */
    (initproc)OwUsbObject_init,/* tp_init */
    0,                         /* tp_alloc */
    0,                         /* tp_new */
};


/**************************************************
 * Iterator
 **************************************************/



static int
OwDevIter_init(OwDevIter *self, PyObject *args, PyObject *kwds)
{
	return 0;
}

PyObject * 
ow_iter(OwDevIter *self, PyObject *args, PyObject *kw)
{
	int r;
	uint8_t owdev[8];

	if (!self->init) {
		r = owusb_search_first(self->dev, self->cmd, owdev);
		self->init = 1;
	} else {
		r = owusb_search_next(self->dev, owdev);
	}
	if (r != 1) {
		Py_RETURN_NONE;
	}
	return PyString_FromStringAndSize((char *)owdev, 8);
}


static PyTypeObject OwDevIterType = {
    PyObject_HEAD_INIT(NULL)
    0,                         /*ob_size*/
    "owusb.DevIter",              /*tp_name*/
    sizeof(OwDevIter),         /*tp_basicsize*/
    0,                         /*tp_itemsize*/
    0,                         /*tp_dealloc*/
    0,                         /*tp_print*/
    0,                         /*tp_getattr*/
    0,                         /*tp_setattr*/
    0,                         /*tp_compare*/
    0,                         /*tp_repr*/
    0,                         /*tp_as_number*/
    0,                         /*tp_as_sequence*/
    0,                         /*tp_as_mapping*/
    0,                         /*tp_hash */
    ow_iter,                   /*tp_call*/
    0,                         /*tp_str*/
    0,                         /*tp_getattro*/
    0,                         /*tp_setattro*/
    0,                         /*tp_as_buffer*/
    Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,        /*tp_flags*/
    "1-Wire device iterator",    /* tp_doc */
    0,		               /* tp_traverse */
    0,		               /* tp_clear */
    0,		               /* tp_richcompare */
    0,		               /* tp_weaklistoffset */
    0,		               /* tp_iter */
    0,		               /* tp_iternext */
    0,                         /* tp_methods */
    0,                         /* tp_members */
    0,                         /* tp_getset */
    0,                         /* tp_base */
    0,                         /* tp_dict */
    0,                         /* tp_descr_get */
    0,                         /* tp_descr_set */
    0,                         /* tp_dictoffset */
    (initproc)OwDevIter_init,  /* tp_init */
    0,                         /* tp_alloc */
    0,                         /* tp_new */
};

/*****************************************************
 * Module methods
 *****************************************************/

static PyMethodDef module_methods[] = {
	{ NULL }
};

/*********************************************************/

PyMODINIT_FUNC
initowusb(void) 
{
	PyObject* m;
	
	OwUsbType.tp_new = PyType_GenericNew;
	if (PyType_Ready(&OwUsbType) < 0) {
		return;
	}
	OwDevIterType.tp_new = PyType_GenericNew;
	if (PyType_Ready(&OwDevIterType) < 0) {
		return;
	}
	m = Py_InitModule3("owusb", module_methods, "1-wire interface");
	
	if (m == NULL) 
		return;

	Py_INCREF(&OwUsbType);
	PyModule_AddObject(m, "OwUsb", (PyObject *)&OwUsbType);
	Py_INCREF(&OwDevIterType);
	PyModule_AddObject(m, "DevIter", (PyObject *)&OwDevIterType);

	owusb_init();
}
