#include <Python.h>

#define PYSTRING(s) PYSTRING_##s

PyObject* PYSTRING(GET);
PyObject* PYSTRING(REQUEST_METHOD);
PyObject* PYSTRING(PATH_INFO);
PyObject* PYSTRING(QUERY_STRING);
PyObject* PYSTRING(CONTENT_TYPE);
PyObject* PYSTRING(HTTP_CONTENT_TYPE);
PyObject* PYSTRING(response_headers);
PyObject* PYSTRING(response_status);
PyObject* PYSTRING(Content_Type);
PyObject* PYSTRING(Content_Length);
PyObject* PYSTRING(groupdict);

void staticstrings_init();
