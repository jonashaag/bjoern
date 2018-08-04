#ifndef _PY2PY3_H
#define _PY2PY3_H

#if PY_MAJOR_VERSION >= 3
#define NOP do{}while(0)
#define PyFile_IncUseCount(file) NOP
#define PyFile_DecUseCount(file) NOP
#define _Bytes_AS_DATA(bytes) PyBytes_AS_STRING(bytes)
#define _Bytes_FromString(bytes) PyBytes_FromString(bytes)
#define _Bytes_FromStringAndSize(data, len) PyBytes_FromStringAndSize(data, len)
#define _Bytes_GET_SIZE(bytes) PyBytes_GET_SIZE(bytes)
#define _Bytes_Check(bytes) PyBytes_Check(bytes)
#define _Bytes_Resize(bytes, len) _PyBytes_Resize(bytes, len)
#define _BytesLatin1_FromUnicode(u) PyUnicode_AsLatin1String(u)
#define _UnicodeUTF8_FromString(data) PyUnicode_FromString(data)
#define _Unicode_GET_SIZE(u) PyUnicode_GET_LENGTH(u)
#define _UnicodeLatin1_FromStringAndSize(data, len) PyUnicode_DecodeLatin1(data, len, "replace")
#define _Unicode_Concat(u1, u2) PyUnicode_Concat(u1, u2)
#define _FromLong(n) PyLong_FromLong(n)

#else

#define _Bytes_AS_DATA(bytes) PyString_AS_STRING(bytes)
#define _Bytes_FromString(bytes) PyString_FromString(bytes)
#define _Bytes_FromStringAndSize(data, len) PyString_FromStringAndSize(data, len)
#define _Bytes_GET_SIZE(bytes) PyString_GET_SIZE(bytes)
#define _Bytes_Check(bytes) PyString_Check(bytes)
#define _Bytes_Resize(bytes, len) _PyString_Resize(bytes, len)
#define _BytesLatin1_FromUnicode(u) (Py_INCREF(u),u)
#define _FromLong(n) PyInt_FromLong(n)

#define _UnicodeUTF8_FromString(data) PyString_FromString(data)
#define _UnicodeLatin1_FromString(name) PyString_FromString(name)
#define _Unicode_GET_SIZE(u) PyString_GET_SIZE(u)

static PyObject* _UnicodeLatin1_FromStringAndSize(const char* data, Py_ssize_t len)
{
  PyObject* tmp = PyUnicode_DecodeLatin1(data, len, "replace");
  if (tmp == NULL) {
    return NULL;
  }
  PyObject* tmp2 = PyUnicode_AsLatin1String(tmp);
  Py_DECREF(tmp);
  return tmp2;
}

static PyObject *_Unicode_Concat(PyObject *l, PyObject *r)
{
  PyObject *ret = l;

  Py_INCREF(l);  /* reference to old left will be stolen */
  PyString_Concat(&ret, r);

  return ret;
}
#endif

#endif /* _PY2PY3_H */
