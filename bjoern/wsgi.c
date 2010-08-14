#include "server.h"
#include "wsgi.h"

bool
wsgi_call_application(Request* request)
{
    request->response = PyString_FromString("Hello World!\n");
    request->state = REQUEST_WSGI_RESPONSE;
    return true;
}

bool
wsgi_send_response(Request* request)
{
    sendall(request, PyString_AS_STRING(request->response), PyString_GET_SIZE(request->response));
    request->state = REQUEST_WSGI_DONE;
    return true;
}
