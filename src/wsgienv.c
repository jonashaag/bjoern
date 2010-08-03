#include "wsgienv.h"

static inline void
copy_wsgi_header(char* restrict dest, c_char* restrict src, c_size_t len)
{
    for(unsigned int i=0; i<len; ++i) {
        if(src[i] == '-') *dest++ = '_';
        else *dest++ = toupper(src[i]);
    }
    *dest++ = '\0';
}

/*
    Transform the current header name to something WSGI (CGI) compatible, e.g.
        User-Agent => HTTP_USER_AGENT
    and store it in the `wsgi_environ` dictionary.
*/
void Environ_SetItemWithLength(PyObject* env,
                               c_char* name,
                               c_size_t name_length_,
                               c_char* value,
                               c_size_t value_length)
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
