#include "filewrapper.h"

static PyObject*
FileWrapper_New(PyObject* self, PyObject* args, PyObject* kwargs)
{
  PyObject* file;
  if(!PyArg_ParseTuple(args, "O:FileWrapper", &file))
    return NULL;
  if(!PyFile_Check(file)) {
    TYPE_ERROR("FileWrapper argument", "file", file);
    return NULL;
  }
  Py_INCREF(file);
  FileWrapper* wrapper = PyObject_NEW(FileWrapper, &FileWrapper_Type);
  PyFile_IncUseCount((PyFileObject*)file);
  wrapper->file = file;
  return (PyObject*)wrapper;
}

static PyObject*
FileWrapper_GetAttrO(PyObject* self, PyObject* name)
{
  return PyObject_GetAttr(((FileWrapper*)self)->file, name);
}

static PyObject*
FileWrapper_Iter(PyObject* self)
{
  return PyObject_GetIter(((FileWrapper*)self)->file);
}

static void
FileWrapper_dealloc(PyObject* self)
{
  PyFile_DecUseCount((PyFileObject*)((FileWrapper*)self)->file);
  Py_DECREF(((FileWrapper*)self)->file);
  PyObject_FREE(self);
}

PyTypeObject FileWrapper_Type = {
  PyVarObject_HEAD_INIT(NULL, 0)
  "FileWrapper",                    /* tp_name (__name__)                     */
  sizeof(FileWrapper),              /* tp_basicsize                           */
  0,                                /* tp_itemsize                            */
  (destructor)FileWrapper_dealloc,  /* tp_dealloc                             */
};

void _init_filewrapper()
{
  FileWrapper_Type.tp_new = FileWrapper_New;
  FileWrapper_Type.tp_iter = FileWrapper_Iter;
  FileWrapper_Type.tp_getattro = FileWrapper_GetAttrO;
  FileWrapper_Type.tp_flags |= Py_TPFLAGS_DEFAULT;
}
