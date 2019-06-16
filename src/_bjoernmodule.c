#include <Python.h>
#include <stdio.h>
#include "server.h"
#include "wsgi.h"
#include "filewrapper.h"
#include "py3.h"


char *slice_str(const char *str, size_t size, size_t start, size_t end) {
    char *buffer = malloc(sizeof(char[size]));
    size_t j = 0;
    for (size_t i = start; i <= end; ++i) {
        buffer[j++] = str[i];
    }
    buffer[j] = 0;
    return buffer;
}

static PyObject *
run(PyObject *self, PyObject *args) {
    ServerInfo info;

    PyObject *socket;
    if (!PyArg_ParseTuple(args, "OOOOO:server_run",
                          &socket,
                          &info.wsgi_app,
                          &info.max_body_len,
                          &info.max_header_fields,
                          &info.max_header_field_len)) {
        return NULL;
    }

    // Check socket
    info.sockfd = PyObject_AsFileDescriptor(socket);
    if (info.sockfd < 0) {
        return NULL;
    }

    info.host = NULL;
    if (PyObject_HasAttrString(socket, "getsockname")) {
        PyObject *sockname = PyObject_CallMethod(socket, "getsockname", NULL);
        if (sockname == NULL) {
            return NULL;
        }
        if (PyTuple_CheckExact(sockname) && PyTuple_GET_SIZE(sockname) == 2) {
            /* Standard (ipaddress, port) case */
            info.host = PyTuple_GET_ITEM(sockname, 0);
            info.port = PyTuple_GET_ITEM(sockname, 1);
        }
    }
    PyObject *objectsRepresentation = PyObject_Repr(info.host);
    PyObject *str = PyUnicode_AsEncodedString(objectsRepresentation, "utf-8", "~E~");
    char *host = PyBytes_AS_STRING(str);
    char *fmt_host = slice_str(host, strlen(host), 1, strlen(host) - 2);
    Py_DECREF(objectsRepresentation);
    Py_DECREF(str);
    free(fmt_host);

    // Action starts
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
