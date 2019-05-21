#ifndef _PY3_H
#define _PY3_H

#define NOP do{}while(0)
#define PyFile_IncUseCount(file) NOP
#define PyFile_DecUseCount(file) NOP

#define _FromLong(n) PyLong_FromLong(n)
#define _Size_t_FromLong(n) PyLong_AsSize_t(n)

#define _PEP3333_Bytes_AS_DATA(bytes) PyBytes_AS_STRING(bytes)
#define _PEP3333_Bytes_FromString(bytes) PyBytes_FromString(bytes)
#define _PEP3333_Bytes_FromStringAndSize(data, len) PyBytes_FromStringAndSize(data, len)
#define _PEP3333_Bytes_GET_SIZE(bytes) PyBytes_GET_SIZE(bytes)
#define _PEP3333_Bytes_Check(bytes) PyBytes_Check(bytes)
#define _PEP3333_Bytes_Resize(bytes, len) _PyBytes_Resize(bytes, len)
#define _PEP3333_BytesLatin1_FromUnicode(u) PyUnicode_AsLatin1String(u)

#define _PEP3333_StringFromFormat(...) PyUnicode_FromFormat(__VA_ARGS__)
#define _PEP3333_String_FromUTF8String(data) PyUnicode_FromString(data)
#define _PEP3333_String_FromLatin1StringAndSize(data, len) PyUnicode_DecodeLatin1(data, len, "replace")
#define _PEP3333_String_GET_SIZE(u) PyUnicode_GET_LENGTH(u)
#define _PEP3333_String_Concat(u1, u2) PyUnicode_Concat(u1, u2)


#endif /* _PY3_H */
