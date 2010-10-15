#include <Python.h>
#include "server.h"
#include "wsgi.h"
#include "bjoernmodule.h"

static PyObject*
run(PyObject* self, PyObject* args)
{
    static bool server_runs = false;

    if(server_runs) {
        PyErr_SetString(
            PyExc_RuntimeError,
            "Only one bjoern server per Python interpreter is allowed"
        );
        return NULL;
    }

    const char* host;
    int port;

    if(!PyArg_ParseTuple(args, "Osi", &wsgi_app, &host, &port))
        return NULL;

    _request_module_initialize(host, port);

    server_runs = true;
    server_run(host, port);
    server_runs = false;

    Py_RETURN_NONE;
}

static PyMethodDef Bjoern_FunctionTable[] = {
    {"run", run, METH_VARARGS, NULL},
    {NULL,  NULL, 0, NULL}
};

PyMODINIT_FUNC initbjoern()
{
    int ready = PyType_Ready(&StartResponse_Type);
    assert(ready == 0);
    assert(StartResponse_Type.tp_flags & Py_TPFLAGS_READY);
    Py_InitModule("bjoern", Bjoern_FunctionTable);
}
