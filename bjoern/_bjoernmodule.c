#include <Python.h>
#include "server.h"
#include "wsgi.h"
#include "filewrapper.h"

#ifdef WANT_STATSD
#include "statsd-client.h"
#endif

static PyObject*
run(PyObject* self, PyObject* args)
{
  ServerInfo info;

  PyObject* socket;

#ifdef WANT_STATSD
  info.statsd = NULL;
  int statsd_enabled;
  char* statsd_host;
  int statsd_port;
  char* statsd_ns;
  char* statsd_tags = NULL;

  if(!PyArg_ParseTuple(args, "OOiziz|z:server_run", &socket, &info.wsgi_app,
                       &statsd_enabled, &statsd_host, &statsd_port, &statsd_ns, &statsd_tags)) {
    return NULL;
  }
#else
  char* ignored_str = NULL;
  int ignored_int = 0;

  if(!PyArg_ParseTuple(args, "OO|izizz:server_run", &socket, &info.wsgi_app, &ignored_int,
                       &ignored_str, &ignored_int, &ignored_str, &ignored_str)) {
    return NULL;
  }
  if (ignored_str != NULL || ignored_int != 0) {
    PyErr_Format(PyExc_TypeError, "Unexpected statsd_* arguments (forgot to compile with statsd support?)");
    return NULL;
  }
#endif

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

#ifdef WANT_STATSD
  if (statsd_enabled) {
      if (statsd_host == NULL || *statsd_host == '\0') {
        statsd_host = "127.0.0.1";
      }

      if (statsd_ns == NULL || *statsd_ns == '\0') {
        info.statsd = statsd_init(statsd_host, statsd_port);
      } else {
        info.statsd = statsd_init_with_namespace(statsd_host, statsd_port, statsd_ns);
      }
#ifdef WANT_STATSD_TAGS
      info.statsd_tags = statsd_tags;
      DBG("Statsd: host=%s, port=%d, ns=%s, tags=%s", statsd_host, statsd_port, statsd_ns, statsd_tags);
#else
      DBG("Statsd: host=%s, port=%d, ns=%s", statsd_host, statsd_port, statsd_ns);
#endif
  } else {
      DBG("Statsd disabled");
  }


#endif

  _initialize_request_module(&info);

  server_run(&info);

#ifdef WANT_STATSD
  statsd_finalize(info.statsd);
#endif

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

  PyObject* features = PyDict_New();

#ifdef WANT_SIGNAL_HANDLING
  PyDict_SetItemString(features, "has_signal_handling", Py_True);
#else
  PyDict_SetItemString(features, "has_signal_handling", Py_False);
#endif

#ifdef WANT_SIGINT_HANDLING
  PyDict_SetItemString(features, "has_sigint_handling", Py_True);
#else
  PyDict_SetItemString(features, "has_sigint_handling", Py_False);
#endif

#ifdef WANT_STATSD
  PyDict_SetItemString(features, "has_statsd", Py_True);
#else
  PyDict_SetItemString(features, "has_statsd", Py_False);
#endif

#ifdef WANT_STATSD_TAGS
  PyDict_SetItemString(features, "has_statsd_tags", Py_True);
#else
  PyDict_SetItemString(features, "has_statsd_tags", Py_False);
#endif

#if PY_MAJOR_VERSION >= 3
  PyObject* bjoern_module = PyModule_Create(&module);
  if (bjoern_module == NULL) {
    return NULL;
  }
#else
  PyObject* bjoern_module = Py_InitModule("_bjoern", Bjoern_FunctionTable);
#endif

  PyModule_AddObject(bjoern_module, "features", features);
  PyModule_AddObject(bjoern_module, "version", Py_BuildValue("(iii)", 3, 2, 2));

#if PY_MAJOR_VERSION >= 3
  return bjoern_module;
#endif
}
