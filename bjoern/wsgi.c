#include "common.h"
#include "server.h"
#include "bjoernmodule.h"
#include "wsgi.h"

static PyKeywordFunc start_response;
static bool wsgi_sendfile(Request*);
static bool wsgi_senditer(Request*);

typedef struct {
    PyObject_HEAD
    Request* request;
} StartResponse;
static PyTypeObject StartResponse_Type;


bool
wsgi_call_application(Request* request)
{
    StartResponse* start_response = PyObject_NEW(StartResponse, &StartResponse_Type);
    start_response->request = request;
    PyObject* args = PyTuple_Pack(/* size */ 2, request->headers, start_response);
    PyObject* retval = PyObject_CallObject(wsgi_app, args);
    Py_DECREF(start_response);

    if(retval == NULL)
        return false;

    if(PyFile_Check(retval)) {
        request->state = REQUEST_WSGI_FILE_RESPONSE;
        request->response = retval;
        return true;
    }

    if(PyString_Check(retval)) {
        request->state = REQUEST_WSGI_STRING_RESPONSE;
        request->response = retval;
        return true;
    }

    PyObject* iter = PyObject_GetIter(retval);
    TYPECHECK2(iter, PyIter, PySeqIter, "wsgi application return value", false);

    Py_DECREF(retval);
    request->state = REQUEST_WSGI_ITER_RESPONSE;
    request->response = iter;
    /* Get the first item of the iterator, because that may execute code that
     * invokes `start_response` (which might not have been invoked yet).
     * Think of the following scenario:
     *
     *     def app(environ, start_response):
     *         start_response('200 Ok', ...)
     *         yield 'Hello World'
     *
     * That would make `app` return an iterator (more precisely, a generator).
     * Unfortunately, `start_response` wouldn't be called until the first item
     * of that iterator is requested; `start_response` however has to be called
     * _before_ the wsgi body is sent, because it passes the HTTP headers.
     */
    request->response_curiter = PyIter_Next(iter);

    return true;
}

bool
wsgi_send_response(Request* request)
{
    switch(request->state) {
        case REQUEST_WSGI_STRING_RESPONSE:
            sendall(
                request,
                PyString_AS_STRING(request->response),
                PyString_GET_SIZE(request->response)
            );
            return true;

        case REQUEST_WSGI_FILE_RESPONSE:
            return wsgi_sendfile(request);

        case REQUEST_WSGI_ITER_RESPONSE:
            return wsgi_senditer(request);

        default:
            assert(0);
    }
}

static bool
wsgi_sendfile(Request* request)
{
    assert(0);
}

static bool
wsgi_senditer(Request* request)
{
    if(request->response_curiter == NULL)
        return true;

    TYPECHECK(request->response_curiter, PyString, "wsgi iterable items", true);
    sendall(
        request,
        PyString_AS_STRING(request->response_curiter),
        PyString_GET_SIZE(request->response_curiter)
    );

    GIL_LOCK(0);
    request->response_curiter = PyIter_Next(request->response);
    GIL_UNLOCK(0);
    return request->response_curiter != NULL;
}



static PyObject*
start_response(PyObject* self, PyObject* args, PyObject* kwargs)
{
    Request* req = ((StartResponse*)self)->request;

    if(!PyArg_UnpackTuple(args,
            /* Python function name */ "start_response",
            /* min args */ 2, /* max args */ 2,
            &req->status,
            &req->response_headers
        ))
        return NULL;

    TYPECHECK(req->status, PyString, "start_response argument 1", NULL);
    TYPECHECK(req->response_headers, PyList, "start_response argument 2", NULL);

    Py_RETURN_NONE;
}

static PyTypeObject StartResponse_Type = {
    PyVarObject_HEAD_INIT(NULL, 0)
    "start_response",
    sizeof(StartResponse),
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    start_response /* __call__ */
};
