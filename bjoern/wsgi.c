#include "common.h"
#include "bjoernmodule.h"
#include "wsgi.h"

#define TYPE_ERROR_INNER(what, expected, ...) \
  PyErr_Format(PyExc_TypeError, what " must be " expected " " __VA_ARGS__)
#define TYPE_ERROR(what, expected, got) \
  TYPE_ERROR_INNER(what, expected, "(got '%.200s' object instead)", Py_TYPE(got)->tp_name)

static PyObject* (start_response)(PyObject* self, PyObject* args, PyObject *kwargs);
static Py_ssize_t wsgi_getheaders(Request*, PyObject* buf);
static inline bool inspect_headers(Request*);
static inline bool should_keep_alive(Request*);

typedef struct {
  PyObject_HEAD
  Request* request;
} StartResponse;

bool
wsgi_call_application(Request* request)
{
  assert(StartResponse_Type.tp_flags & Py_TPFLAGS_READY);
  StartResponse* start_response = PyObject_NEW(StartResponse, &StartResponse_Type);
  start_response->request = request;

  /* From now on, `headers` stores the _response_ headers
   * (passed by the WSGI app) rather than the _request_ headers */
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
   *   def app(environ, start_response):
   *     start_response('200 Ok', ...)
   *     yield 'Hello World'
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
  PyObject* first_chunk;

  if(PyList_Check(retval) && PyList_GET_SIZE(retval) == 1 &&
     PyString_Check(PyList_GET_ITEM(retval, 0)))
  {
    /* Optimize the most common case, a single string in a list: */
    PyObject* tmp = PyList_GET_ITEM(retval, 0);
    Py_INCREF(tmp);
    Py_DECREF(retval);
    retval = tmp;
    goto string; /* eeevil */
  } else if(PyString_Check(retval)) {
    /* According to PEP 333 strings should be handled like any other iterable,
     * i.e. sending the response item for item. "item for item" means
     * "char for char" if you have a string. -- I'm not that stupid. */
    string:
    if(PyString_GET_SIZE(retval)) {
      first_chunk = retval;
    } else {
      Py_DECREF(retval);
      first_chunk = NULL;
    }
  } else {
    /* Generic iterable (list of length != 1, generator, ...) */
    request->iterable = retval;
    request->iterator = PyObject_GetIter(retval);
    if(request->iterator == NULL)
      return false;
    first_chunk = wsgi_iterable_get_next_chunk(request);
    if(first_chunk == NULL && PyErr_Occurred())
      return false;
  }

  if(request->headers == NULL) {
    /* It is important that this check comes *after* the call to
     * wsgi_iterable_get_next_chunk(), because in case the WSGI application
     * was an iterator, there's no chance start_response could be called
     * before. See above if you don't understand what I say. */
    PyErr_SetString(
      PyExc_RuntimeError,
      "wsgi application returned before start_response was called"
    );
    Py_DECREF(first_chunk);
    return false;
  }

  if(should_keep_alive(request)) {
    request->state.chunked_response = request->state.response_length_unknown;
    request->state.keep_alive = true;
  } else {
    request->state.keep_alive = false;
  }

  /* Get the headers and concatenate the first body chunk.
   * In the first place this makes the code more simple because afterwards
   * we can throw away the first chunk PyObject; but it also is an optimization:
   * At least for small responses, the complete response could be sent with
   * one send() call (in server.c:ev_io_on_write) which is a (tiny) performance
   * booster because less kernel calls means less kernel call overhead. */
  PyObject* buf = PyString_FromStringAndSize(NULL, 1024);
  Py_ssize_t length = wsgi_getheaders(request, buf);
  if(length == 0) {
    Py_DECREF(first_chunk);
    Py_DECREF(buf);
    return false;
  }

  if(first_chunk == NULL) {
    _PyString_Resize(&buf, length);
    goto out;
  }

  if(request->state.chunked_response) {
    PyObject* new_chunk = wrap_http_chunk_cruft_around(first_chunk);
    Py_DECREF(first_chunk);
    assert(PyString_GET_SIZE(new_chunk) >= PyString_GET_SIZE(first_chunk) + 5);
    first_chunk = new_chunk;
  }

  assert(buf);
  _PyString_Resize(&buf, length + PyString_GET_SIZE(first_chunk));
  memcpy(PyString_AS_STRING(buf)+length, PyString_AS_STRING(first_chunk),
         PyString_GET_SIZE(first_chunk));

  Py_DECREF(first_chunk);

out:
  request->state.wsgi_call_done = true;
  request->current_chunk = buf;
  request->current_chunk_p = 0;
  return true;
}

static inline bool
inspect_headers(Request* request)
{
  Py_ssize_t i;
  PyObject* tuple;

  for(i=0; i<PyList_GET_SIZE(request->headers); ++i) {
    tuple = PyList_GET_ITEM(request->headers, i);

    if(!PyTuple_Check(tuple) || PyTuple_GET_SIZE(tuple) != 2)
      goto err;

    PyObject* field = PyTuple_GET_ITEM(tuple, 0);
    PyObject* value = PyTuple_GET_ITEM(tuple, 1);

    if(!PyString_Check(field) || !PyString_Check(value))
      goto err;

    if(!strncasecmp(PyString_AS_STRING(field), "Content-Length", PyString_GET_SIZE(field)))
      request->state.response_length_unknown = false;
  }
  return true;

err:
  TYPE_ERROR_INNER("start_response argument 2", "a list of 2-tuples",
    "(found invalid '%.200s' object at position %d)", Py_TYPE(tuple)->tp_name, i);
  return false;
}

static Py_ssize_t
wsgi_getheaders(Request* request, PyObject* buf)
{
  char* bufp = PyString_AS_STRING(buf);

  #define buf_write(src, len) \
    do { \
      size_t n = len; \
      const char* s = src;  \
      while(n--) *bufp++ = *s++; \
    } while(0)
  #define buf_write2(src) buf_write(src, strlen(src))

  buf_write2("HTTP/1.1 ");
  buf_write(PyString_AS_STRING(request->status),
        PyString_GET_SIZE(request->status));

  for(Py_ssize_t i=0; i<PyList_GET_SIZE(request->headers); ++i) {
    PyObject *tuple = PyList_GET_ITEM(request->headers, i);
    PyObject *field = PyTuple_GET_ITEM(tuple, 0),
         *value = PyTuple_GET_ITEM(tuple, 1);
    buf_write2("\r\n");
    buf_write(PyString_AS_STRING(field), PyString_GET_SIZE(field));
    buf_write2(": ");
    buf_write(PyString_AS_STRING(value), PyString_GET_SIZE(value));
  }
  if(request->state.chunked_response)
    buf_write2("\r\nTransfer-Encoding: chunked");
  buf_write2("\r\n\r\n");

  return bufp - PyString_AS_STRING(buf);
}

inline PyObject*
wsgi_iterable_get_next_chunk(Request* request)
{
  /* Get the next item out of ``request->iterable``, skipping empty ones. */
  PyObject* next;
  while(true) {
    next = PyIter_Next(request->iterator);
    if(next == NULL)
      return NULL;
    if(!PyString_Check(next)) {
      TYPE_ERROR("wsgi iterable items", "strings", next);
      Py_DECREF(next);
      return NULL;
    }
    if(PyString_GET_SIZE(next))
      return next;
    Py_DECREF(next);
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

  if(request->state.start_response_called) {
    /* not the first call of start_response --
     * throw away any previous status and headers. */
    Py_CLEAR(request->status);
    Py_CLEAR(request->headers);
    request->state.response_length_unknown = false;
  }

  PyObject* status = NULL;
  PyObject* headers = NULL;
  PyObject* exc_info = NULL;
  if(!PyArg_UnpackTuple(args, "start_response", 2, 3, &status, &headers, &exc_info))
    return NULL;

  if(exc_info) {
    if(!PyTuple_Check(exc_info) || PyTuple_GET_SIZE(exc_info) != 3) {
      TYPE_ERROR("start_response argument 3", "a 3-tuple", exc_info);
      return NULL;
    }

    restore_exception_tuple(exc_info, /* incref items? */ true);

    if(request->state.wsgi_call_done) {
      /* Too late to change headers. According to PEP 333, we should let
       * the exception propagate in this case. */
      return NULL;
    }

    /* Headers not yet sent; handle this start_response call as if 'exc_info'
     * would not have been passed, but print and clear the exception. */
    PyErr_Print();
  }
  else if(request->state.start_response_called) {
    PyErr_SetString(PyExc_TypeError, "'start_response' called twice without "
                     "passing 'exc_info' the second time");
    return NULL;
  }

  if(!PyString_Check(status)) {
    TYPE_ERROR("start_response argument 1", "a 'status reason' string", status);
    return NULL;
  }
  if(!PyList_Check(headers)) {
    TYPE_ERROR("start response argument 2", "a list of 2-tuples", headers);
    return NULL;
  }

  request->headers = headers;

  if(!inspect_headers(request)) {
    request->headers = NULL;
    return NULL;
  }

  request->status = status;

  Py_INCREF(request->status);
  Py_INCREF(request->headers);

  request->state.start_response_called = true;

  Py_RETURN_NONE;
}

PyTypeObject StartResponse_Type = {
  PyObject_HEAD_INIT(NULL)
  0,                          /* ob_size (deprecated)                       */
  "start_response",           /* tp_name (__name__)                         */
  sizeof(StartResponse),      /* tp_basicsize                               */
  0,                          /* tp_itemsize                                */
  (destructor)PyObject_FREE,  /* tp_dealloc                                 */
  0, 0, 0, 0, 0, 0, 0, 0, 0,  /* tp_{print,getattr,setattr,compare,...}     */
  start_response              /* tp_call (__call__)                         */
};

#define F_KEEP_ALIVE 1<<1
#define have_http11(parser) (parser.http_major > 0 && parser.http_minor > 0)

static inline bool
should_keep_alive(Request* request)
{
  if(!(request->parser.parser.flags & F_KEEP_ALIVE)) {
    /* Only keep-alive if the client requested it explicitly */
    return false;
  }
  if(request->state.response_length_unknown) {
    /* If the client wants to keep-alive the connection but we don't know
     * the response length, we can use Transfer-Encoding: chunked on HTTP/1.1.
     * On HTTP/1.0 no such thing exists so there's no other option than closing
     * the connection to indicate the response end. */
    return have_http11(request->parser.parser);
  } else {
    /* If the response length is known we can keep-alive for both 1.0 and 1.1 */
    return true;
  }
}

PyObject*
wrap_http_chunk_cruft_around(PyObject* chunk)
{
  /* Who the hell decided to use decimal representation for Content-Length
   * but hexadecimal representation for chunk lengths btw!?! Fuck W3C */
  size_t chunklen = PyString_GET_SIZE(chunk);
  assert(chunklen);
  char buf[strlen("ffffffffffffffff") + 2];
  size_t n = sprintf(buf, "%x\r\n", chunklen);
  PyObject* new_chunk = PyString_FromStringAndSize(NULL, n + chunklen + 2);
  char* new_chunk_p = PyString_AS_STRING(new_chunk);
  memcpy(new_chunk_p, buf, n);
  new_chunk_p += n;
  memcpy(new_chunk_p, PyString_AS_STRING(chunk), chunklen);
  new_chunk_p += chunklen;
  *new_chunk_p++ = '\r'; *new_chunk_p = '\n';
  assert(new_chunk_p == PyString_AS_STRING(new_chunk) + n + chunklen + 1);
  return new_chunk;
}

void _initialize_wsgi_module() {
  int ready = PyType_Ready(&StartResponse_Type);
  assert(ready == 0);
  assert(StartResponse_Type.tp_flags & Py_TPFLAGS_READY);
}
