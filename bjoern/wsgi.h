
#ifndef BJOERN_WSGI_H_
#define BJOERN_WSGI_H_ 1

#include "request.h"

PyTypeObject StartResponse_Type;

bool wsgi_call_application(Request *);
PyObject *wsgi_iterable_get_next_chunk(Request *);

#endif /* !BJOERN_WSGI_H_ */
