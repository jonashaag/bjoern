#include "request.h"
#include "parser.h"

Request* Request_new()
{
    Request* request = calloc(1, sizeof(Request));
    request->parser = Parser_new();
    request->parser->request = request;
    return request;
}

void Request_free(Request* request)
{
    GIL_LOCK();
    Py_DECREF(request->response_body);
    Py_XDECREF(request->request_environ);
#ifdef WANT_ROUTING
    Py_XDECREF(request->route_kwargs);
#endif
    GIL_UNLOCK();
    free(request);
}
