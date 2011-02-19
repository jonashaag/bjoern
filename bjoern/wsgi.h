#include <Python.h>
#include "request.h"

bool wsgi_call_application(Request*);
PyObject* wsgi_iterable_get_next_chunk(Request*);
PyObject* wrap_http_chunk_cruft_around(PyObject* chunk);

PyTypeObject StartResponse_Type;
