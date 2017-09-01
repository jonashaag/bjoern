#include <Python.h>
#include "server.h"
#include "wsgi.h"
#include "filewrapper.h"

static PyObject*
run(PyObject* self, PyObject* args)
{
  ServerInfo info;

  PyObject* socket;

  if(!PyArg_ParseTuple(args, "OO:server_run", &socket, &info.wsgi_app)) {
    return NULL;
  }

  info.sockfd = PyObject_AsFileDescriptor(socket);
  if (info.sockfd < 0) {
    return NULL;
  }

  info.host = NULL;
  if (PyObject_HasAttrString(socket, "getsockname")) {
    PyObject* sockname = PyObject_CallMethod(socket, "getsockname", NULL);
    if (sockname == NULL) {
      return NULL;
    }
    if (PyTuple_CheckExact(sockname) && PyTuple_GET_SIZE(sockname) == 2) {
      /* Standard (ipaddress, port) case */
      info.host = PyTuple_GET_ITEM(sockname, 0);
      info.port = PyTuple_GET_ITEM(sockname, 1);
    }
  }

  _initialize_request_module();
  server_run(&info);

  Py_RETURN_NONE;
}

static PyMethodDef Bjoern_FunctionTable[] = {
  {"server_run", (PyCFunction) run, METH_VARARGS, NULL},
  {NULL, NULL, 0, NULL}
};

#if PY_MAJOR_VERSION >= 3
static struct PyModuleDef module = {
  PyModuleDef_HEAD_INIT,
  "bjoern",
  NULL,
  -1, /* size of per-interpreter state of the module,
         or -1 if the module keeps state in global variables. */
  Bjoern_FunctionTable,
  NULL, NULL, NULL, NULL,
};
#endif

#if PY_MAJOR_VERSION >= 3
  #define INIT_BJOERN PyInit__bjoern
#else
  #define INIT_BJOERN init_bjoern
#endif

PyMODINIT_FUNC INIT_BJOERN(void)
{
  _init_common();
  _init_filewrapper();

  PyType_Ready(&FileWrapper_Type);
  assert(FileWrapper_Type.tp_flags & Py_TPFLAGS_READY);
  PyType_Ready(&StartResponse_Type);
  assert(StartResponse_Type.tp_flags & Py_TPFLAGS_READY);
  Py_INCREF(&FileWrapper_Type);
  Py_INCREF(&StartResponse_Type);

#if PY_MAJOR_VERSION >= 3
  PyObject* bjoern_module = PyModule_Create(&module);
  if (bjoern_module == NULL) {
    return NULL;
  }

  PyModule_AddObject(bjoern_module, "version", Py_BuildValue("(iii)", 2, 0, 1));
  return bjoern_module;
#else
  PyObject* bjoern_module = Py_InitModule("_bjoern", Bjoern_FunctionTable);
  PyModule_AddObject(bjoern_module, "version", Py_BuildValue("(iii)", 2, 0, 1));
#endif

}
