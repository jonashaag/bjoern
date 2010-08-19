#include "common.h"
#include "server.h"
#include "bjoernmodule.h"
#include "wsgi.h"

static PyKeywordFunc start_response;
static bool wsgi_sendheaders(Request*);
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

    request->response_headers = NULL;
    PyObject* retval = PyObject_CallFunctionObjArgs(
        wsgi_app,
        request->headers,
        start_response,
        NULL /* sentinel */
    );

    if(retval == NULL)
        return false;

    if(!request->response_headers) {
        PyErr_SetString(
            PyExc_TypeError,
            "wsgi application returned before start_response was called"
        );
        return false;
    }

    /* Optimize the most common case: */
    if(PyList_Check(retval) && PyList_GET_SIZE(retval) == 1) {
        PyObject* tmp = retval;
        retval = PyList_GET_ITEM(tmp, 0);
        Py_DECREF(tmp);
        goto string_resp; /* eeeeeevil */
    }


    if(PyFile_Check(retval)) {
        request->state = REQUEST_WSGI_FILE_RESPONSE;
        request->response = retval;
        return true;
    }

    if(PyString_Check(retval)) {
string_resp:
        request->state = REQUEST_WSGI_STRING_RESPONSE;
        request->response = retval;
        return true;
    }

    PyObject* iter = PyObject_GetIter(retval);
    Py_DECREF(retval);
    TYPECHECK2(iter, PyIter, PySeqIter, "wsgi application return value", false);

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
    if(request->response_headers) {
        if(wsgi_sendheaders(request))
            return true;
        request->response_headers = NULL;
    }

    switch(request->state) {
        case REQUEST_WSGI_STRING_RESPONSE:
            return !sendall(
                request,
                PyString_AS_STRING(request->response),
                PyString_GET_SIZE(request->response)
            );

        case REQUEST_WSGI_FILE_RESPONSE:
            return wsgi_sendfile(request);

        case REQUEST_WSGI_ITER_RESPONSE:
            return wsgi_senditer(request);

        default:
            assert(0);
    }
}

static bool
wsgi_sendheaders(Request* request)
{
    char buf[1024*4];
    size_t bufpos = 0;
    #define buf_write(src, len) \
        do { \
            size_t n = len; \
            const char* s = src;  \
            while(n--) buf[bufpos++] = *s++; \
        } while(0)

    buf_write("HTTP/1.1 ", strlen("HTTP/1.1 "));
    buf_write(PyString_AS_STRING(request->status),
              PyString_GET_SIZE(request->status));

    size_t n_headers = PyList_GET_SIZE(request->response_headers);
    for(size_t i=0; i<n_headers; ++i) {
        PyObject* tuple = PyList_GET_ITEM(request->response_headers, i);
        TYPECHECK(tuple, PyTuple, "headers", true);

        if(PyTuple_GET_SIZE(tuple) < 2) {
            PyErr_Format(
                PyExc_TypeError,
                "headers must be tuples of length 2, not %d",
                PyTuple_GET_SIZE(tuple)
            );
            return true;
        }
        PyObject* field = PyTuple_GET_ITEM(tuple, 0);
        PyObject* value = PyTuple_GET_ITEM(tuple, 1);
        TYPECHECK(field, PyString, "header tuple items", true);
        TYPECHECK(value, PyString, "header tuple items", true);

        buf_write("\r\n", strlen("\r\n"));
        buf_write(PyString_AS_STRING(field), PyString_GET_SIZE(field));
        buf_write(": ", strlen(": "));
        buf_write(PyString_AS_STRING(value), PyString_GET_SIZE(value));
    }
    buf_write("\r\n\r\n", strlen("\r\n\r\n"));

    return !sendall(request, buf, bufpos);
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
    if(!sendall(
        request,
        PyString_AS_STRING(request->response_curiter),
        PyString_GET_SIZE(request->response_curiter)
    )) return true;

    request->response_curiter = PyIter_Next(request->response);
    return request->response_curiter == NULL;
}



static PyObject*
start_response(PyObject* self, PyObject* args, PyObject* kwargs)
{
    Request* req = ((StartResponse*)self)->request;

    if(!PyArg_UnpackTuple(args, "start_response", 2, 2,
                          &req->status, &req->response_headers))
        return NULL;

    TYPECHECK(req->status, PyString, "start_response argument 1", NULL);
    TYPECHECK(req->response_headers, PyList, "start_response argument 2", NULL);

    Py_INCREF(req->status);
    Py_INCREF(req->response_headers);

    Py_RETURN_NONE;
}

static PyTypeObject StartResponse_Type = {
    PyVarObject_HEAD_INIT(NULL, 0)
    "start_response",
    sizeof(StartResponse),
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    start_response /* __call__ */
};
