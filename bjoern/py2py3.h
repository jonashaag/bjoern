#ifndef _PY2PY3_H
#define _PY2PY3_H

#if PY_MAJOR_VERSION >= 3
#define NOP do{}while(0)
#define PyFile_IncUseCount(file) NOP
#define PyFile_DecUseCount(file) NOP
#define _Bytes_AS_DATA(obj) PyBytes_AS_STRING(obj)
#define _Unicode_AS_DATA(obj) PyUnicode_AsUTF8(obj)
#define _Bytes_FromString(name) PyBytes_FromString(name)
#define _Unicode_FromString(name) PyUnicode_FromString(name)
#define _Bytes_FromStringAndSize(data, len) PyBytes_FromStringAndSize(data, len)
#define _Unicode_FromStringAndSize(data, len) PyUnicode_FromStringAndSize(data, len)
#define _Bytes_GET_SIZE(obj) PyBytes_GET_SIZE(obj)
#define _Unicode_GET_SIZE(obj) PyUnicode_GET_LENGTH(obj)
#define _Bytes_Check(obj) PyBytes_Check(obj)
#define _Unicode_Check(obj) PyUnicode_Check(obj)
#define _Bytes_Resize(obj, len) _PyBytes_Resize(obj, len)
#define _Unicode_Resize(obj, len) PyUnicode_Resize(obj, len)
#define _FromLong(n) PyLong_FromLong(n)
#define _File_Check(file) (PyObject_HasAttrString(file, "fileno") && \
                           PyCallable_Check(PyObject_GetAttrString(file, "fileno")))
#else
#define _Bytes_AS_DATA(obj) PyString_AS_STRING(obj)
#define _Unicode_AS_DATA(obj) PyString_AS_STRING(obj)
#define _Bytes_FromString(name) PyString_FromString(name)
#define _Unicode_FromString(name) PyString_FromString(name)
#define _Bytes_FromStringAndSize(data, len) PyString_FromStringAndSize(data, len)
#define _Unicode_FromStringAndSize(data, len) PyString_FromStringAndSize(data, len)
#define _Bytes_GET_SIZE(obj) PyString_GET_SIZE(obj)
#define _Unicode_GET_SIZE(obj) PyString_GET_SIZE(obj)
#define _Bytes_Check(obj) PyString_Check(obj)
#define _Unicode_Check(obj) PyString_Check(obj)
#define _Bytes_Resize(obj, len) _PyString_Resize(obj, len)
#define _Unicode_Resize(obj, len) _PyString_Resize(obj, len)
#define _FromLong(n) PyInt_FromLong(n)
#define _File_Check(file) PyFile_Check(file)
#endif

#endif /* _PY2PY3_H */
