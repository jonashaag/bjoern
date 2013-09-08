#include <Python.h>
#include "server.h"
#include "wsgi.h"
#include "bjoernmodule.h"
#include "filewrapper.h"


PyDoc_STRVAR(listen_doc,
"listen(wsgi_app, host, [port, [reuse_port=False]]) -> None\n\n \
\
Makes bjoern listen to 'host:port' and use 'wsgi_app' as WSGI application. \
(This does not run the server mainloop.)\n\n \
\
'reuse_port' -- whether to set SO_REUSEPORT (if available on platform)");
static PyObject*
listen(PyObject* self, PyObject* args, PyObject* kwds)
{
  static char* keywords[] = {"wsgi_app", "host", "port", "reuse_port", NULL};

  const char* host;
  int port = 0;
  int reuse_port = false; // see footnote (1)

  if(wsgi_app) {
    PyErr_SetString(
      PyExc_RuntimeError,
      "Only one bjoern server per Python interpreter is allowed"
    );
    return NULL;
  }

  if(!PyArg_ParseTupleAndKeywords(args, kwds, "Os|ii:run/listen", keywords,
                                  &wsgi_app, &host, &port, &reuse_port))
    return NULL;

  _initialize_request_module(host, port);

  if(!port) {
    /* Unix socket: "unix:/tmp/foo.sock" or "unix:@abstract-socket" (Linux) */
    if(strncmp("unix:", host, 5)) {
      PyErr_Format(PyExc_ValueError, "'port' missing but 'host' is not a Unix socket");
      goto err;
    }
    host += 5;
  }

  if(!server_init(host, port, reuse_port)) {
    if(port)
      PyErr_Format(PyExc_RuntimeError, "Could not start server on %s:%d", host, port);
    else
      PyErr_Format(PyExc_RuntimeError, "Could not start server on %s", host);
    goto err;
  }

  Py_RETURN_NONE;

err:
  wsgi_app = NULL;
  return NULL;
}

PyDoc_STRVAR(run_doc,
"run(*args, **kwargs) -> None\n \
Calls listen(*args, **kwargs) and starts the server mainloop.\n \
\n\
run() -> None\n \
Starts the server mainloop. listen(...) has to be called before calling \
run() without arguments.");
static PyObject*
run(PyObject* self, PyObject* args, PyObject* kwds)
{
  if(PyTuple_GET_SIZE(args) == 0) {
    /* bjoern.run() */
    if(!wsgi_app) {
      PyErr_SetString(
        PyExc_RuntimeError,
        "Must call bjoern.listen(wsgi_app, host, ...) before "
        "calling bjoern.run() without arguments."
      );
      return NULL;
    }
  } else {
    /* bjoern.run(wsgi_app, host, ...) */
    if(!listen(self, args, kwds))
      return NULL;
  }

  server_run();
  wsgi_app = NULL;
  Py_RETURN_NONE;
}

static PyMethodDef Bjoern_FunctionTable[] = {
  {"run",     (PyCFunction) run,    METH_VARARGS | METH_KEYWORDS, run_doc},
  {"listen",  (PyCFunction) listen, METH_VARARGS | METH_KEYWORDS, listen_doc},
  {NULL, NULL, 0, NULL}
};

PyMODINIT_FUNC initbjoern(void)
{
  _init_common();
  _init_filewrapper();

  PyType_Ready(&FileWrapper_Type);
  assert(FileWrapper_Type.tp_flags & Py_TPFLAGS_READY);
  PyType_Ready(&StartResponse_Type);
  assert(StartResponse_Type.tp_flags & Py_TPFLAGS_READY);

  PyObject* bjoern_module = Py_InitModule("bjoern", Bjoern_FunctionTable);
  PyModule_AddObject(bjoern_module, "version", Py_BuildValue("(iii)", 1, 3, 3));
}

/* (1) Don't use bool here because we use the "i" type specifier in the call to
 * PyArg_ParseTuple below which always writes sizeof(int) bytes (and thus
 * might write to memory regions we don't expect it to write to. */
