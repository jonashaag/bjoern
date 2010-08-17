#include <Python.h>
#include "server.h"
#include "bjoernmodule.h"

static PyObject*
run(PyObject* self, PyObject* args)
{
    const char* host;
    size_t port;

    if(!PyArg_ParseTuple(args, "Osi", &wsgi_app, &host, &port))
        return NULL;

    server_run(host, port);
    Py_RETURN_NONE;
}

static PyMethodDef Bjoern_FunctionTable[] = {
    {"run", run, METH_VARARGS, NULL},
    {NULL,  NULL, 0, NULL}
};


PyMODINIT_FUNC initbjoern()
{
    Py_InitModule("bjoern", Bjoern_FunctionTable);
}
