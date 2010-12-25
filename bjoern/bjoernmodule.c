/**
 * bjoernmodule.c: Python module.
 * Copyright (c) 2010 Jonas Haag <jonas@lophus.org> and contributors.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <Python.h>
#include "server.h"
#include "wsgi.h"
#include "bjoernmodule.h"


PyDoc_STRVAR(listen_doc,
"listen(application, host, port) -> None\n\n \
\
Makes bjoern listen to host:port and use application as WSGI callback. \
(This does not run the server mainloop.)");
static PyObject*
listen(PyObject* self, PyObject* args)
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

    if(!PyArg_ParseTuple(args, "Osi:run/listen", &wsgi_app, &host, &port))
        return NULL;

    _request_module_initialize(host, port);

    if(!server_init(host, port)) {
        PyErr_Format(
            PyExc_RuntimeError,
            "Could not start server on %s:%d", host, port
        );
        return NULL;
    }

    Py_RETURN_NONE;
}

PyDoc_STRVAR(run_doc,
"run(application, host, port) -> None\n \
Calls listen(application, host, port) and starts the server mainloop.\n \
\n\
run() -> None\n \
Starts the server mainloop. listen(...) has to be called before calling \
run() without arguments.");
static PyObject*
run(PyObject* self, PyObject* args)
{
    if(PyTuple_GET_SIZE(args) == 0) {
        // bjoern.run()
        if(!wsgi_app) {
            PyErr_SetString(
                PyExc_RuntimeError,
                "Must call bjoern.listen(app, host, port) before "
                "calling bjoern.run() without arguments."
            );
            return NULL;
        }
    } else {
        // bjoern.run(app, host, port)
        if(!listen(self, args))
            return NULL;
    }

    server_run();
    wsgi_app = NULL;
    Py_RETURN_NONE;
}

static PyMethodDef Bjoern_FunctionTable[] = {
    {"run", run, METH_VARARGS, run_doc},
    {"listen", listen, METH_VARARGS, listen_doc},
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
