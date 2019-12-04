#ifndef _PY2PY3_H
#define _PY2PY3_H

#if PY_MAJOR_VERSION >= 3
#define NOP do{}while(0)
#define PyFile_IncUseCount(file) NOP
#define PyFile_DecUseCount(file) NOP
#define _FromLong(n) PyLong_FromLong(n)
#define _PEP3333_Bytes_AS_DATA(bytes) PyBytes_AS_STRING(bytes)
#define _PEP3333_Bytes_FromString(bytes) PyBytes_FromString(bytes)
#define _PEP3333_Bytes_FromStringAndSize(data, len) PyBytes_FromStringAndSize(data, len)
#define _PEP3333_Bytes_GET_SIZE(bytes) PyBytes_GET_SIZE(bytes)
#define _PEP3333_Bytes_Check(bytes) PyBytes_Check(bytes)
#define _PEP3333_Bytes_Resize(bytes, len) _PyBytes_Resize(bytes, len)
#define _PEP3333_BytesLatin1_FromUnicode(u) PyUnicode_AsLatin1String(u)
#define _PEP3333_String_FromUTF8String(data) PyUnicode_FromString(data)
#define _PEP3333_String_FromLatin1StringAndSize(data, len) PyUnicode_DecodeLatin1(data, len, "replace")
// Shouldn't use FromFormat here because of Python bug:
// https://bugs.python.org/issue33817
// While problem with PyUnicode_FromString("") was not reported, it's still better
// to create empty string without formatting
#define _PEP3333_String_Empty() PyUnicode_FromString("")
#define _PEP3333_String_FromFormat(...) PyUnicode_FromFormat(__VA_ARGS__)
#define _PEP3333_String_GET_SIZE(u) PyUnicode_GET_LENGTH(u)
#define _PEP3333_String_Concat(u1, u2) PyUnicode_Concat(u1, u2)
#define _PEP3333_String_CompareWithASCIIString(o, c_str) PyUnicode_CompareWithASCIIString(o, c_str)

#else

#define _FromLong(n) PyInt_FromLong(n)
#define _PEP3333_Bytes_AS_DATA(bytes) PyString_AS_STRING(bytes)
#define _PEP3333_Bytes_FromString(bytes) PyString_FromString(bytes)
#define _PEP3333_Bytes_FromStringAndSize(data, len) PyString_FromStringAndSize(data, len)
#define _PEP3333_Bytes_GET_SIZE(bytes) PyString_GET_SIZE(bytes)
#define _PEP3333_Bytes_Check(bytes) PyString_Check(bytes)
#define _PEP3333_Bytes_Resize(bytes, len) _PyString_Resize(bytes, len)
#define _PEP3333_BytesLatin1_FromUnicode(u) (Py_INCREF(u),u)
#define _PEP3333_String_FromUTF8String(data) PyString_FromString(data) // Assume UTF8
// Can't use FromFormat here because of Python bug:
// https://bugs.python.org/issue33817
// Arguments (NULL, 0) will create empty string, it's explicitly allowed
#define _PEP3333_String_Empty() PyString_FromStringAndSize(NULL, 0)
#define _PEP3333_String_FromFormat(...) PyString_FromFormat(__VA_ARGS__)
#define _PEP3333_String_GET_SIZE(u) PyString_GET_SIZE(u)

static PyObject* _PEP3333_String_FromLatin1StringAndSize(const char* data, Py_ssize_t len)
{
  PyObject* tmp = PyUnicode_DecodeLatin1(data, len, "replace");
  if (tmp == NULL) {
    return NULL;
  }
  PyObject* tmp2 = PyUnicode_AsLatin1String(tmp);
  Py_DECREF(tmp);
  return tmp2;
}

static PyObject *_PEP3333_String_Concat(PyObject *l, PyObject *r)
{
  PyObject *ret = l;

  Py_INCREF(l);  /* reference to old left will be stolen */
  PyString_Concat(&ret, r);

  return ret;
}

static int _PEP3333_String_CompareWithASCIIString(PyObject *o, const char *c_str)
{
  return memcmp(_PEP3333_Bytes_AS_DATA(o), c_str, _PEP3333_Bytes_GET_SIZE(o));
}
#endif

#endif /* _PY2PY3_H */
