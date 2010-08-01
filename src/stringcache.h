/*
    Some predefined, often-required string objects.
*/

#ifndef __stringcache__
  #define __stringcache__
  #define PYSTRING(s) PYSTRING_##s
  #define wuerg(s) static PyObject* PYSTRING(s)
  wuerg(Content_Type);
  wuerg(groupdict);
#else
  #undef wuerg
  #define wuerg(s) PYSTRING(s) = PyString_FromString(#s)
  PYSTRING(Content_Type)    = PyString_FromString("Content-Type");
  PYSTRING(groupdict)       = PyString_FromString("groupdict");
#endif

wuerg(GET);
wuerg(REQUEST_METHOD);
wuerg(PATH_INFO);
wuerg(QUERY_STRING);
wuerg(CONTENT_TYPE);
wuerg(HTTP_CONTENT_TYPE);
wuerg(response_headers);
wuerg(response_status);
