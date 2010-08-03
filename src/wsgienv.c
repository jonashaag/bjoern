#include "bjoern.h"

static inline void
copy_wsgi_header(char* restrict destination,
                 const char* restrict source,
                 const size_t length)
{
    for(unsigned int i=0; i<length; ++i) {
        if(source[i] == '-') *destination++ = '_';
        else *destination++ = toupper(source[i]);
    }
    *destination++ = '\0';
}

/*
    Transform the current header name to something WSGI (CGI) compatible, e.g.
        User-Agent => HTTP_USER_AGENT
    and store it in the `wsgi_environ` dictionary.
*/
void
Environ_SetItemWithLength(PyObject* env,
                          const char* name, const size_t name_length_,
                          const char* value, const size_t value_length)
{
    size_t name_length = name_length_ + strlen("HTTP_");
    PyObject* header_name = PyString_FromStringAndSize(NULL /* empty string */,
                                                       name_length + 1);
    memcpy(PyString_AS_STRING(header_name), "HTTP_", 5);
    copy_wsgi_header(
        PyString_AS_STRING(header_name),
        name,
        name_length
    );
    PyObject* header_value = PyString_FromStringAndSize(value, value_length);
    PyDict_SetItem(env, header_name, header_value);

    Py_DECREF(header_name);
    Py_DECREF(header_value);
}
