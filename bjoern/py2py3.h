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
#define _PEP3333_String_GET_SIZE(u) PyUnicode_GET_LENGTH(u)
#define _PEP3333_String_Concat(u1, u2) PyUnicode_Concat(u1, u2)

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
#endif

#endif /* _PY2PY3_H */
