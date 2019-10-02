#include <Python.h>
#include "server.h"
#include "wsgi.h"
#include "filewrapper.h"
#include "statsd-client.h"

static PyObject*
run(PyObject* self, PyObject* args)
{
  ServerInfo info;

  PyObject* socket;

#ifdef WANT_STATSD
  StatsdInfo statsd_info;
  char* statsd_tags = NULL;

  if(!PyArg_ParseTuple(args, "OOiziz|z:server_run", &socket, &info.wsgi_app,
                       &statsd_info.enabled, &statsd_info.host, &statsd_info.port,
                       &statsd_info.namespace, &statsd_tags)) {
    return NULL;
  }
#else
  char* ignored_str;
  int ignored_int;

  if(!PyArg_ParseTuple(args, "OO|izizz:server_run", &socket, &info.wsgi_app, &ignored_int,
                       &ignored_str, &ignored_int, &ignored_str, &ignored_str)) {
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
  statsd_link* statsd = NULL;
  if (statsd_info.enabled) {
      if (statsd_info.host == NULL || *statsd_info.host == '\0') {
        statsd_info.host = "127.0.0.1";
      }

      if (statsd_info.namespace == NULL || *statsd_info.namespace == '\0') {
        statsd = statsd_init(statsd_info.host, statsd_info.port);
      } else {
        statsd = statsd_init_with_namespace(statsd_info.host, statsd_info.port, statsd_info.namespace);
      }
  }
#endif

  _initialize_request_module(&info);

#ifdef WANT_STATSD
  #ifdef WANT_STATSD_TAGS
  server_run(&info, statsd, statsd_tags);
  #else
  server_run(&info, statsd);
  #endif
  statsd_finalize(statsd);
#else
  server_run(&info);
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

#if PY_MAJOR_VERSION >= 3
  PyObject* bjoern_module = PyModule_Create(&module);
  if (bjoern_module == NULL) {
    return NULL;
  }

  PyModule_AddObject(bjoern_module, "version", Py_BuildValue("(iii)", 3, 0, 1));
  return bjoern_module;
#else
  PyObject* bjoern_module = Py_InitModule("_bjoern", Bjoern_FunctionTable);
  PyModule_AddObject(bjoern_module, "version", Py_BuildValue("(iii)", 3, 0, 1));
#endif

}
