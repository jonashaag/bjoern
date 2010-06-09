/*
    Some predefined, often-required string objects.
*/

#ifndef __stringcache__
  #define __stringcache__
  #define PYSTRING(s) PYSTRING_##s
  #define wuerg(s) static PyObject* PYSTRING(s)
  wuerg(Content_Type);
  wuerg(500_INTERNAL_SERVER_ERROR);
#else
  #undef wuerg
  #define wuerg(s) PYSTRING(s) = PyString_FromString(#s)
  PYSTRING(Content_Type)     = PyString_FromString("Content-Type");
  PYSTRING(500_INTERNAL_SERVER_ERROR) = PyString_FromString("500 Internal Server Error :(");
#endif

wuerg(GET);
wuerg(REQUEST_METHOD);
wuerg(PATH_INFO);
wuerg(QUERY_STRING);
wuerg(CONTENT_TYPE);
wuerg(HTTP_CONTENT_TYPE);
wuerg(response_headers);
wuerg(response_status);
