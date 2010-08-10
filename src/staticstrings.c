#include "staticstrings.h"

PyObject*
http_method_py_str(enum http_method m)
{
    if(m > SIZEOF(py_http_methods))
        return PyString_FromString(http_method_str(m));
    else
        return py_http_methods[m];
}

void
staticstrings_init()
{
    for(short i=0; i<SIZEOF(py_http_methods); ++i) {
        py_http_methods[i] = PyString_FromString((const char*)py_http_methods[i]);
    }

    #define init(s) _(s) = PyString_FromString(#s)
    init(GET);
    init(REQUEST_METHOD);
    init(PATH_INFO);
    init(QUERY_STRING);
    init(CONTENT_TYPE);
    init(HTTP_CONTENT_TYPE);
    init(response_headers);
    init(response_status);
    init(groupdict);
    init(close);
    init(__len__);
    #undef init
    _(Content_Length) = PyString_FromString("Content-Length");
    _(Content_Type) = PyString_FromString("Content-Type");
    _static_empty_pystring = PyString_FromString("");
}
