#include "staticstrings.h"

#define init(s) _(s) = PyString_FromString(#s)

void
staticstrings_init()
{
    for(short i=0; i<sizeof(py_http_methods); ++i)
        py_http_methods[i] = PyString_FromString((const char*)py_http_methods[i]);

    init(GET);
    init(REQUEST_METHOD);
    init(PATH_INFO);
    init(QUERY_STRING);
    init(CONTENT_TYPE);
    init(HTTP_CONTENT_TYPE);
    init(response_headers);
    init(response_status);
    init(groupdict);
    _(Content_Length) = PyString_FromString("Content-Length");
    _(Content_Type) = PyString_FromString("Content-Type");
    _static_empty_pystring = PyString_FromString("");
}

#undef init
