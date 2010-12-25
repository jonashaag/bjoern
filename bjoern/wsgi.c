/**
 * wsgi.c: WSGI functions.
 * Copyright (c) 2010 Jonas Haag <jonas@lophus.org> and contributors.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include "common.h"
#include "server.h"
#include "bjoernmodule.h"
#include "wsgi.h"

static PyKeywordFunc start_response;
static Py_ssize_t wsgi_getheaders(Request*, PyObject* buf);

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

    /* The following code is somewhat magic, so worth an explanation.
     *
     * If the application we called was a generator, we have to call .next() on
     * it before we do anything else because that may execute code that
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
     *
     * If the application returned a list this would not be required of course,
     * but special-handling is painful - especially in C - so here's one generic
     * way to solve the problem:
     *
     * Look into the returned iterator in any case. This allows us to do other
     * optimizations, for example if the returned value is a list with exactly
     * one string in it, we can pick the string and throw away the list so bjoern
     * does not have to come back again and look into the iterator a second time.
     */
    PyObject* first_chunk = NULL;

    if(PyString_Check(retval)) {
        /* According to PEP 333 strings should be handled like any other iterable,
           i.e. sending the response item for item. "item for item" means
           "char for char" if you have a string. -- I'm not that stupid. */
        first_chunk = retval;
    }
    else if(PyList_Check(retval) && PyList_GET_SIZE(retval) == 1) {
        /* Optimize the most common case, a single string in a list: */
        first_chunk = PyList_GET_ITEM(retval, 0);
        Py_INCREF(first_chunk);
        Py_DECREF(retval);
    } else {
        /* Generic iterable (list of length != 1, generator, ...) */
        PyObject* iter = PyObject_GetIter(retval);
        Py_DECREF(retval);
        TYPECHECK2(iter, PyIter, PySeqIter, "wsgi application return value", false);
        request->iterable = iter;
        first_chunk = wsgi_iterable_get_next_chunk(request);
        if(first_chunk == NULL && PyErr_Occurred())
            return false;
    }

    if(request->headers == NULL) {
        /* It is important that this check comes *after* the call to
           wsgi_iterable_get_next_chunk(), because in case the WSGI application
           was an iterator, there's no chance start_response could be called
           before. See above if you don't understand what I say. */
        PyErr_SetString(
            PyExc_TypeError,
            "wsgi application returned before start_response was called"
        );
        Py_DECREF(first_chunk);
        return false;
    }

    /* Get the headers and concatenate the first body chunk to them.
       In the first place this makes the code more simple because afterwards
       we can throw away the first chunk PyObject; but it also is an optimization:
       At least for small responses, the complete response could be sent with
       one send() call (in server.c:ev_io_on_write) which is a (tiny) performance
       booster because less kernel calls means less kernel call overhead. */
    PyObject* buf = PyString_FromStringAndSize(NULL, 1024);
    Py_ssize_t length = wsgi_getheaders(request, buf);
    if(length == 0) {
        Py_DECREF(first_chunk);
        Py_DECREF(buf);
        return false;
    }

    if(first_chunk == NULL) {
        /* The iterator returned by the application was empty. No body. */
        _PyString_Resize(&buf, length);
        goto out;
    }

    assert(first_chunk);
    assert(buf);
    _PyString_Resize(&buf, length + PyString_GET_SIZE(first_chunk));
    memcpy(PyString_AS_STRING(buf)+length, PyString_AS_STRING(first_chunk),
           PyString_GET_SIZE(first_chunk));

    Py_DECREF(first_chunk);

out:
    request->state.headers_sent = true;
    request->current_chunk = buf;
    request->current_chunk_p = 0;
    return true;
}

static Py_ssize_t
wsgi_getheaders(Request* request, PyObject* buf)
{
    register char* bufp = PyString_AS_STRING(buf);

    #define buf_write(src, len) \
        do { \
            size_t n = len; \
            const char* s = src;  \
            while(n--) *bufp++ = *s++; \
        } while(0)

    buf_write("HTTP/1.0 ", strlen("HTTP/1.0 "));
    buf_write(PyString_AS_STRING(request->status),
              PyString_GET_SIZE(request->status));

    PyObject *tuple, *field, *value;
    for(Py_ssize_t i=0; i<PyList_GET_SIZE(request->headers); ++i) {
        tuple = PyList_GET_ITEM(request->headers, i);
        assert(tuple);
        TYPECHECK(tuple, PyTuple, "headers", 0);

        if(PyTuple_GET_SIZE(tuple) != 2) {
            PyErr_Format(
                PyExc_TypeError,
                "headers must be tuples of length 2, not %zd",
                PyTuple_GET_SIZE(tuple)
            );
            return 0;
        }

        field = PyTuple_GET_ITEM(tuple, 0);
        value = PyTuple_GET_ITEM(tuple, 1);

        TYPECHECK(field, PyString, "header tuple items", 0);
        TYPECHECK(value, PyString, "header tuple items", 0);

        buf_write("\r\n", strlen("\r\n"));
        buf_write(PyString_AS_STRING(field), PyString_GET_SIZE(field));
        buf_write(": ", strlen(": "));
        buf_write(PyString_AS_STRING(value), PyString_GET_SIZE(value));
    }
    buf_write("\r\n\r\n", strlen("\r\n\r\n"));

    return bufp - PyString_AS_STRING(buf);
}

inline PyObject*
wsgi_iterable_get_next_chunk(Request* request)
{
    PyObject* next = PyIter_Next(request->iterable);
    if(next)
        TYPECHECK(next, PyString, "wsgi iterable items", NULL);
    return next;
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

    if(request->state.start_response_called) {
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

        if(request->state.headers_sent) {
            /* Headers already sent. According to PEP 333, we should
             * let the exception propagate in this case. */
            return NULL;
        }

        /* Headers not yet sent; handle this start_response call as if 'exc_info'
           would not have been passed, but print and clear the exception. */
        PyErr_Print();
    }
    else if(request->state.start_response_called) {
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
    request->state.start_response_called = true;

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
