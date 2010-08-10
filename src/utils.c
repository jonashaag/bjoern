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

size_t
uitoa10(uint32_t value, char* str)
{
    char* wstr = str;
    do {
        *wstr++ = (char)(48 + (value % 10));
    } while (value /= 10);
    strreverse(str, wstr-1);
    return wstr-str;
}


bool
validate_header_list(PyObject* list)
{
    PyObject* bloedmann;
    const char* errmsg;

    if(list == NULL || !PyList_Check(list)) {
       errmsg = "response_headers must be a list, not %.200s";
       bloedmann = list ? list : Py_None;
       goto error;
    }

    PyObject* item;
    size_t header_list_length = PyList_GET_SIZE(list);
    for(unsigned int i=0; i<header_list_length; ++i) {
        item = PyList_GET_ITEM(list, i);
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
        bloedmann == Py_None ? "None" : Py_TYPE(bloedmann)->tp_name
    );
    return false;
}
