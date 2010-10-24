#include "common.h"
#include "server.h"
#include "bjoernmodule.h"
#include "wsgi.h"

static PyKeywordFunc start_response;
static inline bool _check_start_response(Request*);
static bool wsgi_sendheaders(Request*);
static inline bool wsgi_sendstring(Request*);
static bool wsgi_sendfile(Request*);
static bool wsgi_senditer(Request*);

typedef struct {
    PyObject_HEAD
    Request* request;
} StartResponse;
PyTypeObject StartResponse_Type;


bool
wsgi_call_application(Request* request)
{
    assert(StartResponse_Type.tp_flags & Py_TPFLAGS_READY);
    StartResponse* start_response = PyObject_NEW(StartResponse, &StartResponse_Type);
    start_response->request = request;

    /* From now on, `headers` stores the _response_ headers
     * (passed by the WSGI app) rather than the _request_ headers
     */
    PyObject* request_headers = request->headers;
    request->headers = NULL;

    /* application(environ, start_response) call */
    PyObject* retval = PyObject_CallFunctionObjArgs(
        wsgi_app,
        request_headers,
        start_response,
        NULL /* sentinel */
    );

    Py_DECREF(request_headers);
    Py_DECREF(start_response);

    if(retval == NULL)
        return false;

    request->state |= REQUEST_RESPONSE_WSGI;

    /* Optimize the most common case: */
    if(PyList_Check(retval) && PyList_GET_SIZE(retval) == 1) {
        PyObject* list = retval;
        retval = PyList_GET_ITEM(list, 0);
        Py_INCREF(retval);
        Py_DECREF(list);
        goto string_resp; /* eeeeeevil */
    }

#if 0
    if(PyFile_Check(retval)) {
        request->state |= REQUEST_WSGI_FILE_RESPONSE;
        request->response = retval;
        goto out;
    }
#endif

    if(PyString_Check(retval)) {
string_resp:
        request->state |= REQUEST_WSGI_STRING_RESPONSE;
        request->response = retval;
        goto out;
    }

    PyObject* iter = PyObject_GetIter(retval);
    Py_DECREF(retval);
    TYPECHECK2(iter, PyIter, PySeqIter, "wsgi application return value", false);

    request->state |= REQUEST_WSGI_ITER_RESPONSE;
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
    request->iterable_next = PyIter_Next(iter);
    if(PyErr_Occurred())
        return false;

out:
    if(!request->headers) {
        PyErr_SetString(
            PyExc_TypeError,
            "wsgi application returned before start_response was called"
        );
        return false;
    }
    return true;
}

bool
wsgi_send_response(Request* request)
{
    if(!(request->state & REQUEST_RESPONSE_HEADERS_SENT)) {
        if(wsgi_sendheaders(request))
            return true;
        request->state |= REQUEST_RESPONSE_HEADERS_SENT;
    }

    request_state state = request->state;
    if(state & REQUEST_WSGI_STRING_RESPONSE)    return wsgi_sendstring(request);
    else if(state & REQUEST_WSGI_FILE_RESPONSE) return wsgi_sendfile(request);
    else if(state & REQUEST_WSGI_ITER_RESPONSE) return wsgi_senditer(request);

    assert(0);
    return 0; /* make gcc happy */
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

    buf_write("HTTP/1.0 ", strlen("HTTP/1.0 "));
    buf_write(PyString_AS_STRING(request->status),
              PyString_GET_SIZE(request->status));

    size_t n_headers = PyList_GET_SIZE(request->headers);
    for(size_t i=0; i<n_headers; ++i) {
        PyObject* tuple = PyList_GET_ITEM(request->headers, i);
        assert(tuple);
        TYPECHECK(tuple, PyTuple, "headers", true);

        if(PyTuple_GET_SIZE(tuple) < 2) {
            PyErr_Format(
                PyExc_TypeError,
                "headers must be tuples of length 2, not %zd",
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

static inline bool
wsgi_sendstring(Request* request)
{
    sendall(
        request,
        PyString_AS_STRING(request->response),
        PyString_GET_SIZE(request->response)
    );
    return true;
}

static bool
wsgi_sendfile(Request* request)
{
    assert(0);
    return 0; /* make gcc happy */
}

static bool
wsgi_senditer(Request* request)
{
#define ITER_MAXSEND 1024*4
    PyObject* item = request->iterable_next;
    if(!item) return true;

    ssize_t sent = 0;
    while(item && sent < ITER_MAXSEND) {
        TYPECHECK(item, PyString, "wsgi iterable items", true);
        if(!sendall(request, PyString_AS_STRING(item),
                    PyString_GET_SIZE(item)))
            return true;
        sent += PyString_GET_SIZE(item);
        Py_DECREF(item);
        item = PyIter_Next(request->response);
        if(PyErr_Occurred()) {
            /* TODO: What to do here? Parts of the response are already sent */
            return true;
        }
    }

    if(item) {
        request->iterable_next = item;
        return false;
    } else {
        return true;
    }
}

static inline void
restore_exception_tuple(PyObject* exc_info, bool incref_items)
{
    if(incref_items) {
        Py_INCREF(PyTuple_GET_ITEM(exc_info, 0));
        Py_INCREF(PyTuple_GET_ITEM(exc_info, 1));
        Py_INCREF(PyTuple_GET_ITEM(exc_info, 2));
    }
    PyErr_Restore(
        PyTuple_GET_ITEM(exc_info, 0),
        PyTuple_GET_ITEM(exc_info, 1),
        PyTuple_GET_ITEM(exc_info, 2)
    );
}

static PyObject*
start_response(PyObject* self, PyObject* args, PyObject* kwargs)
{
    Request* request = ((StartResponse*)self)->request;

    if(request->state & REQUEST_START_RESPONSE_CALLED) {
        /* not the first call of start_response --
           throw away any previous status and headers. */
        Py_DECREF(request->status);
        Py_DECREF(request->headers);
        request->status = NULL;
        request->headers = NULL;
    }

    PyObject* status = NULL;
    PyObject* headers = NULL;
    PyObject* exc_info = NULL;
    if(!PyArg_UnpackTuple(args, "start_response", 2, 3, &status, &headers, &exc_info))
        return NULL;

    if(exc_info) {
        TYPECHECK(exc_info, PyTuple, "start_response argument 3", NULL);
        if(PyTuple_GET_SIZE(exc_info) != 3) {
            PyErr_Format(
                PyExc_TypeError,
                "start_response argument 3 must be a tuple of length 3, "
                "not of length %zd",
                PyTuple_GET_SIZE(exc_info)
            );
            return NULL;
        }

        restore_exception_tuple(exc_info, /* incref items? */ true);

        if(request->state & REQUEST_RESPONSE_HEADERS_SENT)
            /* Headers already sent. According to PEP 333, we should
             * let the exception propagate in this case. */
            return NULL;

        /* Headers not yet sent; handle this start_response call if 'exc_info'
           would not be passed, but print the exception and 'sys.exc_clear()' */
        PyErr_Print();
    }
    else if(request->state & REQUEST_START_RESPONSE_CALLED) {
        PyErr_SetString(PyExc_TypeError, "'start_response' called twice without "
                                         "passing 'exc_info' the second time");
        return NULL;
    }

    TYPECHECK(status, PyString, "start_response argument 1", NULL);
    TYPECHECK(headers, PyList, "start_response argument 2", NULL);

    Py_INCREF(status);
    Py_INCREF(headers);

    request->status = status;
    request->headers = headers;
    request->state |= REQUEST_START_RESPONSE_CALLED;

    Py_RETURN_NONE;
}

PyTypeObject StartResponse_Type = {
    PyObject_HEAD_INIT(NULL)
    0,                          /* ob_size (deprecated)                     */
    "start_response",           /* tp_name (__name__)                       */
    sizeof(StartResponse),      /* tp_basicsize                             */
    0,                          /* tp_itemsize                              */
    (destructor)PyObject_FREE,  /* tp_dealloc                               */
    0, 0, 0, 0, 0, 0, 0, 0, 0,  /* tp_{print,getattr,setattr,compare,...}   */
    start_response              /* tp_call (__call__)                       */
};
