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
    for(int i=0; i<length; ++i)
    {
        if(source[i] == '-')
            *destination++ = '_';
        else
            *destination++ = toupper(source[i]);
    }
    *destination++ = '\0';
}

/*
    Like PyTuple_Pack, but without INCREF-ing the objects passed.
*/
PyObject* PyTuple_Pack_NoINCREF(Py_ssize_t n, ...)
{
    Py_ssize_t i;
    PyObject *o;
    PyObject *result;
    PyObject **items;
    va_list vargs;

    va_start(vargs, n);
    result = PyTuple_New(n);
    if (result == NULL)
        return NULL;
    items = ((PyTupleObject *)result)->ob_item;
    for (i = 0; i < n; i++) {
        o = va_arg(vargs, PyObject *);
        /* Py_INCREF(o); */
        items[i] = o;
    }
    va_end(vargs);
    return result;
}
