#if PY_MAJOR_VERSION >= 3
  #define PyString_Check PyBytes_Check
  #define PyString_FromFormat PyBytes_FromFormat
  #define PyString_FromString PyBytes_FromString
  #define PyString_FromStringAndSize PyBytes_FromStringAndSize
  #define _PyString_Resize _PyBytes_Resize
  #define PyString_AS_STRING PyBytes_AS_STRING
  #define PyString_GET_SIZE PyBytes_GET_SIZE
#endif
