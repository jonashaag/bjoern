#include <Python.h>
#include "server.h"
#include "wsgi.h"
#include "bjoernmodule.h"

PyDoc_STRVAR(listen_doc,
"listen(application, host, port) -> None\n\n \
\
Makes bjoern listen to host:port and use application as WSGI callback. \
(This does not run the server mainloop.)");
static PyObject*
listen(PyObject* self, PyObject* args)
{
  const char* host;
  int port;

  if(wsgi_app) {
    PyErr_SetString(
      PyExc_RuntimeError,
      "Only one bjoern server per Python interpreter is allowed"
    );
    return NULL;
  }

  if(!PyArg_ParseTuple(args, "Osi:run/listen", &wsgi_app, &host, &port))
    return NULL;

  _initialize_request_module(host, port);

  if(!server_init(host, port)) {
    PyErr_Format(
      PyExc_RuntimeError,
      "Could not start server on %s:%d", host, port
    );
    return NULL;
  }

  Py_RETURN_NONE;
}

PyDoc_STRVAR(run_doc,
"run(application, host, port) -> None\n \
Calls listen(application, host, port) and starts the server mainloop.\n \
\n\
run() -> None\n \
Starts the server mainloop. listen(...) has to be called before calling \
run() without arguments.");
static PyObject*
run(PyObject* self, PyObject* args)
{
  if(PyTuple_GET_SIZE(args) == 0) {
    /* bjoern.run() */
    if(!wsgi_app) {
      PyErr_SetString(
        PyExc_RuntimeError,
        "Must call bjoern.listen(app, host, port) before "
        "calling bjoern.run() without arguments."
      );
      return NULL;
    }
  } else {
    /* bjoern.run(app, host, port) */
    if(!listen(self, args))
      return NULL;
  }

  server_run();
  wsgi_app = NULL;
  Py_RETURN_NONE;
}

static PyMethodDef Bjoern_FunctionTable[] = {
  {"run", run, METH_VARARGS, run_doc},
  {"listen", listen, METH_VARARGS, listen_doc},
  {NULL,  NULL, 0, NULL}
};

#if PY_MAJOR_VERSION >= 3
  static struct PyModuleDef moduledef = {
    PyModuleDef_HEAD_INIT,
    "bjoern",
    NULL,
    -1,
    Bjoern_FunctionTable,
    NULL,
    NULL,
    NULL,
    NULL
  };

  PyObject *
  PyInit_bjoern(void)
#else
  void
  initbjoern(void)
#endif
{
  _initialize_wsgi_module();
  _initialize_static_strings();

  #if PY_MAJOR_VERSION >= 3
    PyObject* bjoern_module = PyModule_Create(&moduledef);
  #else
    PyObject* bjoern_module = Py_InitModule("bjoern", Bjoern_FunctionTable);
  #endif

  PyModule_AddObject(bjoern_module, "version", Py_BuildValue("(ii)", 1, 0));

  #if PY_MAJOR_VERSION >= 3
    return bjoern_module;
  #endif
}
