#include "filewrapper.h"
#include "py2py3.h"

#define FW_self ((FileWrapper*)self)

int FileWrapper_GetFd(PyObject *self)
{
  return FW_self->fd;
}

static PyObject*
FileWrapper_New(PyTypeObject* cls, PyObject* args, PyObject* kwargs)
{
  PyObject* file;
  PyObject* blocksize = NULL;

  if(!PyArg_ParseTuple(args, "O|O:FileWrapper", &file, &blocksize))
    return NULL;

  Py_INCREF(file);
  Py_XINCREF(blocksize);

  int fd = PyObject_AsFileDescriptor(file);
  if (fd == -1) {
    PyErr_Clear();
  } else {
    PyFile_IncUseCount((PyFileObject*)file);
  }

  FileWrapper* wrapper = PyObject_NEW(FileWrapper, cls);
  wrapper->file = file;
  wrapper->blocksize = blocksize;
  wrapper->fd = fd;

  return (PyObject*)wrapper;
}

static PyObject*
FileWrapper_Iter(PyObject* self)
{
  Py_INCREF(self);
  return self;
}

static PyObject*
FileWrapper_IterNext(PyObject* self)
{
  PyObject* data = PyObject_CallMethodObjArgs(FW_self->file, _read, FW_self->blocksize, NULL);
  if (data != NULL && PyObject_IsTrue(data)) {
    return data;
  } else {
    return NULL;
  }
}

void FileWrapper_dealloc(PyObject* self)
{
  Py_DECREF(FW_self->file);
  Py_XDECREF(FW_self->blocksize);
  PyObject_FREE(self);
}

PyObject* FileWrapper_close(PyObject* self)
{
  if (PyObject_HasAttr(FW_self->file, _close)) {
    return PyObject_CallMethodObjArgs(FW_self->file, _close, NULL);
  } else {
    Py_RETURN_NONE;
  }
  if (FW_self->fd != -1) {
    PyFile_DecUseCount((PyFileObject*)FW_self->file);
  }
}

static PyMethodDef FileWrapper_methods[] = {
    {"close", (PyCFunction) FileWrapper_close, METH_NOARGS, NULL},
    {NULL}  /* Sentinel */
};

PyTypeObject FileWrapper_Type = {
  PyVarObject_HEAD_INIT(NULL, 0)
  "FileWrapper",                    /* tp_name (__name__)                     */
  sizeof(FileWrapper),              /* tp_basicsize                           */
  0,                                /* tp_itemsize                            */
  (destructor)FileWrapper_dealloc,  /* tp_dealloc                             */
};

void _init_filewrapper(void)
{
  FileWrapper_Type.tp_new = FileWrapper_New;
  FileWrapper_Type.tp_iter = FileWrapper_Iter;
  FileWrapper_Type.tp_iternext = FileWrapper_IterNext;
  FileWrapper_Type.tp_flags |= Py_TPFLAGS_DEFAULT;
  FileWrapper_Type.tp_methods = FileWrapper_methods;
}
