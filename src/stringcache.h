/*
    Some predefined, often-required string objects.
*/

#ifndef __stringcache__
  #define __stringcache__
  #define PYSTRING(s) PYSTRING_##s
  #define wuerg(s) static PyObject* PYSTRING(s)
  wuerg(HTTP_500_MESSAGE);
  wuerg(HTTP_404_MESSAGE);
  wuerg(Content_Type);
#else
  #undef wuerg
  #define wuerg(s) PYSTRING(s) = PyString_FromString(#s)
  PYSTRING(HTTP_500_MESSAGE) = PyString_FromString(HTTP_500_MESSAGE);
  PYSTRING(HTTP_404_MESSAGE) = PyString_FromString(HTTP_404_MESSAGE);
  PYSTRING(Content_Type)     = PyString_FromString("Content-Type");
#endif

wuerg(GET);
wuerg(REQUEST_METHOD);
wuerg(PATH_INFO);
wuerg(QUERY_STRING);
wuerg(CONTENT_TYPE);
wuerg(HTTP_CONTENT_TYPE);
wuerg(_response_headers);
wuerg(response_status);
