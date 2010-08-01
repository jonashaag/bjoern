#include "Python.h"
#include "utils.h"
/*
    bjoern_strcpy:
    like `strcpy`, but updates the `destination` by the number of bytes copied.
    (thus, `destination` is a char pointer pointer / a pointer to a char array.)
*/
static inline void
bjoern_strcpy(char** destination, const char* source)
{
    while (( **destination = *source )) {
        source++;
        (*destination)++;
    };
}

static inline void
bjoern_http_to_wsgi_header(char* destination, const char* source, size_t length)
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
    const char* bloedmann;
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
