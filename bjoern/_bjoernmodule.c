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

PyMODINIT_FUNC init_bjoern(void)
{
  _init_common();
  _init_filewrapper();

  PyType_Ready(&FileWrapper_Type);
  assert(FileWrapper_Type.tp_flags & Py_TPFLAGS_READY);
  PyType_Ready(&StartResponse_Type);
  assert(StartResponse_Type.tp_flags & Py_TPFLAGS_READY);

  PyObject* bjoern_module = Py_InitModule("_bjoern", Bjoern_FunctionTable);
  PyModule_AddObject(bjoern_module, "version", Py_BuildValue("(iii)", 1, 4, 3));
}
