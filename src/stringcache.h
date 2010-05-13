/*
    Some predefined, often-required string objects.
*/

#ifndef __stringcache__
  #define __stringcache__
  #define PYSTRING(s) PYSTRING_##s
  #define wuerg(s) static PyObject* PYSTRING(s)
#else
  #undef wuerg
  #define wuerg(s) PYSTRING(s) = PyString_FromString(#s)
#endif

wuerg(GET);
wuerg(REQUEST_METHOD);
wuerg(PATH_INFO);
wuerg(QUERY_STRING);
wuerg(HTTP_CONTENT_TYPE);
wuerg(CONTENT_TYPE);
//wuerg(404_NOT_FOUND);
//wuerg(500_INTERNAL_SERVER_ERROR);
wuerg(start_response);
wuerg(response_headers);
wuerg(response_status);
