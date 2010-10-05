#include "request.h"

bool wsgi_call_application(Request*);
bool wsgi_send_response(Request*);
static PyTypeObject StartResponse_Type;
