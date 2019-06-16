#include <Python.h>
#include <stdio.h>
#include "server.h"
#include "wsgi.h"
#include "filewrapper.h"
#include "py3.h"


static PyObject *
run(PyObject *self, PyObject *args) {
    ServerInfo info;

    if (!PyArg_ParseTuple(args, "isiOnnn:server_run",
                          &info.sockfd,
                          &info.host,
                          &info.port,
                          &info.wsgi_app,
                          &info.max_body_len,
                          &info.max_header_fields,
                          &info.max_header_field_len)) {
        return NULL;
    }

    // Initialize requests and run the server
    set_server_info(&info);
    _initialize_request_module();
    server_run();

    Py_RETURN_NONE;
}

static PyMethodDef Bjoern_FunctionTable[] = {
        {"server_run", (PyCFunction) run, METH_VARARGS, NULL},
        {NULL,         NULL,              0,            NULL}
};

static struct PyModuleDef module = {
        PyModuleDef_HEAD_INIT,
        "bjoern",
        NULL,
        -1, /* size of per-interpreter state of the module,
         or -1 if the module keeps state in global variables. */
        Bjoern_FunctionTable,
        NULL, NULL, NULL, NULL
};

#define INIT_BJOERN PyInit__bjoern


PyMODINIT_FUNC INIT_BJOERN(void) {
    _init_common();
    _init_filewrapper();

    PyType_Ready(&FileWrapper_Type);
    assert(FileWrapper_Type.tp_flags & Py_TPFLAGS_READY);
    PyType_Ready(&StartResponse_Type);
    assert(StartResponse_Type.tp_flags & Py_TPFLAGS_READY);
    Py_INCREF(&FileWrapper_Type);
    Py_INCREF(&StartResponse_Type);

    PyObject *bjoern_module = PyModule_Create(&module);
    if (bjoern_module == NULL) {
        return NULL;
    }

    return bjoern_module;
}
