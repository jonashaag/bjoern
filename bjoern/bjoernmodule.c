#include <Python.h>
#include "server.h"

static PyMethodDef Bjoern_FunctionTable[] = {
    {NULL,  NULL, 0, NULL}
};


PyMODINIT_FUNC init_bjoern()
{
    Py_InitModule("_bjoern", Bjoern_FunctionTable);
    server_run("0.0.0.0", 8080);
}
