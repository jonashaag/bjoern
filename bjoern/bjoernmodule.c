#include <Python.h>
#include "server.h"
#include "wsgi.h"
#include "bjoernmodule.h"

static PyObject*
run(PyObject* self, PyObject* args)
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

    if(!PyArg_ParseTuple(args, "Osi", &wsgi_app, &host, &port))
        return NULL;

    _request_module_initialize(host, port);

    if(server_init(host, port)) {
        server_run();
        wsgi_app = NULL;
        Py_RETURN_NONE;
    } else {
        PyErr_Format(
            PyExc_RuntimeError,
            "Could not start server on %s:%d", host, port
        );
        return NULL;
    }
}

static PyMethodDef Bjoern_FunctionTable[] = {
    {"run", run, METH_VARARGS, "bjoern.run(application, host, port)"},
    {NULL,  NULL, 0, NULL}
};

PyMODINIT_FUNC initbjoern()
{
    int ready = PyType_Ready(&StartResponse_Type);
    assert(ready == 0);
    assert(StartResponse_Type.tp_flags & Py_TPFLAGS_READY);

    PyObject* bjoern_module = Py_InitModule("bjoern", Bjoern_FunctionTable);
    PyModule_AddObject(bjoern_module, "version", Py_BuildValue("(ii)", 1, 0));
}
