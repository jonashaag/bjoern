#include "request.h"

bool wsgi_call_application(Request*);
PyObject* wsgi_iterable_get_next_chunk(Request*);
PyTypeObject StartResponse_Type;
PyObject* wrap_http_chunk_cruft_around(PyObject* chunk);
