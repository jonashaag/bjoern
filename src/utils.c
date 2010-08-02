#include "Python.h"
#include "utils.h"

/* strreverse and utoa10: from  http://code.google.com/p/stringencoders */

static void
strreverse(char* begin, char* end)
{
    char aux;
    while (end > begin) {
        aux = *end;
        *end-- = *begin;
        *begin++ = aux;
    }
}

static size_t
uitoa10(uint32_t value, char* str)
{
    char* wstr = str;
    do {
        *wstr++ = (char)(48 + (value % 10));
    } while (value /= 10);
    --wstr;
    strreverse(str, wstr);
    return wstr-str;
}


static inline void
http_to_wsgi_header(char* restrict destination,
                    const char* restrict source,
                    const size_t length)
{
    for(unsigned int i=0; i<length; ++i)
    {
        if(source[i] == '-')
            *destination++ = '_';
        else
            *destination++ = toupper(source[i]);
    }
    *destination++ = '\0';
}


static bool
validate_header_tuple(PyObject* tuple)
{
    PyObject* bloedmann;
    const char* errmsg;

    if(tuple == NULL || !PyTuple_Check(tuple)) {
       errmsg = "response_headers must be a tuple";
       bloedmann = tuple ? tuple : Py_None;
       goto error;
    }

    PyObject* item;
    size_t header_tuple_length = PyTuple_GET_SIZE(tuple);
    for(unsigned int i=0; i<header_tuple_length; ++i) {
        item = PyTuple_GET_ITEM(tuple, i);
        if(!PyTuple_Check(item)) {
            errmsg = "response_headers items must be tuples, not %.200s";
            bloedmann = item;
            goto error;
        }
        if(!PyString_Check(PyTuple_GET_ITEM(item, 0))) {
            errmsg = "header-tuple items must be of type str, not %.200s";
            bloedmann = PyTuple_GET_ITEM(item, 0);
            goto error;
        }
        if(!PyString_Check(PyTuple_GET_ITEM(item, 1))) {
            errmsg = "header-tuple items must be of type str, not %.200s";
            bloedmann = PyTuple_GET_ITEM(item, 1);
            goto error;
        }
    }

    return true;

error:
    PyErr_Format(
        PyExc_TypeError,
        errmsg,
        Py_TYPE(bloedmann)->tp_name
    );
    return false;
}
