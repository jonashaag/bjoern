#include "request.h"

bool wsgi_call_application(Request*);
PyObject* wsgi_iterable_get_next_chunk(Request*);
PyTypeObject StartResponse_Type;
