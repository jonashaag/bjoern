#include <Python.h>
#include "server.h"
#include "wsgi.h"
#include "bjoernmodule.h"

static PyObject*
run(PyObject* self, PyObject* args)
{
    const char* host;
    int port;

    if(!PyArg_ParseTuple(args, "Osi", &wsgi_app, &host, &port))
        return NULL;

    _request_module_initialize(host, port);

    server_run(host, port);
    Py_RETURN_NONE;
}

static PyMethodDef Bjoern_FunctionTable[] = {
    {"run", run, METH_VARARGS, NULL},
    {NULL,  NULL, 0, NULL}
};


PyMODINIT_FUNC initbjoern()
{
    int ready = PyType_Ready(&StartResponse_Type);
    assert(ready >= 0);
    Py_InitModule("bjoern", Bjoern_FunctionTable);
}
