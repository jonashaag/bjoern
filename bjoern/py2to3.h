#if PY_MAJOR_VERSION >= 3
  #define PyInt_FromLong PyLong_FromLong
#else
  #define PyBytes_Check PyString_Check
  #define PyBytes_FromFormat PyString_FromFormat
  #define PyBytes_FromString PyString_FromString
  #define PyBytes_FromStringAndSize PyString_FromStringAndSize
  #define _PyBytes_Resize _PyString_Resize
  #define PyBytes_AS_STRING PyString_AS_STRING
  #define PyBytes_GET_SIZE PyString_GET_SIZE

  #define PyUnicode_Check PyString_Check
  #define PyUnicode_FromFormat PyString_FromFormat
  #define PyUnicode_FromString PyString_FromString
  #define PyUnicode_FromStringAndSize PyString_FromStringAndSize
  #define _PyUnicode_Resize _PyString_Resize
  #define PyUnicode_AS_UNICODE PyString_AS_STRING
  #define PyUnicode_GET_SIZE PyString_GET_SIZE
#endif
