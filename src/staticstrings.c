#include "staticstrings.h"

#define INIT_STRING(s) PYSTRING(s) = PyString_FromString(#s)

void
staticstrings_init()
{
    INIT_STRING(GET);
    INIT_STRING(REQUEST_METHOD);
    INIT_STRING(PATH_INFO);
    INIT_STRING(QUERY_STRING);
    INIT_STRING(CONTENT_TYPE);
    INIT_STRING(HTTP_CONTENT_TYPE);
    INIT_STRING(response_headers);
    INIT_STRING(response_status);
    INIT_STRING(groupdict);
    PYSTRING(Content_Length) = PyString_FromString("Content-Length");
    PYSTRING(Content_Type) = PyString_FromString("Content-Type");
}
