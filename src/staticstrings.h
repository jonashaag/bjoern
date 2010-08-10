#include <Python.h>
#include "http_parser.h"

#define SIZEOF(x) ((sizeof(x)/sizeof(x[0])))
#define _(s) _static_pystring_##s

/* those will become REAL PyObjects later in `staticstrings_init` */
static PyObject* py_http_methods[5] = {
    (PyObject*) "DELETE",
    (PyObject*) "GET",
    (PyObject*) "HEAD",
    (PyObject*) "POST",
    (PyObject*) "PUT"
};

PyObject* _(GET);
PyObject* _(REQUEST_METHOD);
PyObject* _(PATH_INFO);
PyObject* _(QUERY_STRING);
PyObject* _(CONTENT_TYPE);
PyObject* _(HTTP_CONTENT_TYPE);
PyObject* _(response_headers);
PyObject* _(response_status);
PyObject* _(Content_Type);
PyObject* _(Content_Length);
PyObject* _(groupdict);
PyObject* _(close);
PyObject* _(__len__);
PyObject* _static_empty_pystring;

void staticstrings_init();
PyObject* http_method_py_str(enum http_method m);
