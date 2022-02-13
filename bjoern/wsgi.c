#include "common.h"
#include "filewrapper.h"
#include "wsgi.h"
#include "py2py3.h"

static void wsgi_getheaders(Request*, PyObject** buf, Py_ssize_t* length);

typedef struct {
  PyObject_HEAD
  Request* request;
} StartResponse;

bool
wsgi_call_application(Request* request)
{
  StartResponse* start_response = PyObject_NEW(StartResponse, &StartResponse_Type);
  start_response->request = request;

  /* From now on, `headers` stores the _response_ headers
   * (passed by the WSGI app) rather than the _request_ headers */
  PyObject* request_headers = request->headers;
  request->headers = NULL;

  /* application(environ, start_response) call */
  PyObject* retval = PyObject_CallFunctionObjArgs(
    request->server_info->wsgi_app,
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
   * one bytestring in it, we can pick the bytestring and throw away the list
   * so bjoern does not have to come back again and look into the iterator a
   * second time.
   */
  PyObject* first_chunk;

  if(PyList_Check(retval) && PyList_GET_SIZE(retval) == 1 &&
     _PEP3333_Bytes_Check(PyList_GET_ITEM(retval, 0)))
  {
    /* Optimize the most common case, a single bytestring in a list: */
    DBG_REQ(request, "WSGI iterable is list of size 1");
    PyObject* tmp = PyList_GET_ITEM(retval, 0);
    Py_INCREF(tmp);
    Py_DECREF(retval);
    retval = tmp;
    goto bytestring; /* eeevil */
  } else if(_PEP3333_Bytes_Check(retval)) {
    /* According to PEP 333 strings should be handled like any other iterable,
     * i.e. sending the response item for item. "item for item" means
     * "char for char" if you have a bytestring. -- I'm not that stupid. */
    bytestring:
    DBG_REQ(request, "WSGI iterable is byte string");
    request->iterable = NULL;
    request->iterator = NULL;
    if(_PEP3333_Bytes_GET_SIZE(retval)) {
      first_chunk = retval;
    } else {
      // empty response
      Py_DECREF(retval);
      first_chunk = NULL;
    }
  } else if(!request->state.response_length_unknown && FileWrapper_CheckExact(retval) && FileWrapper_GetFd(retval) != -1) {
    DBG_REQ(request, "WSGI iterable is wsgi.file_wrapper instance and Content-Length is known");
    request->iterable = retval;
    request->iterator = NULL;
    first_chunk = NULL;
  } else {
    /* Generic iterable (list of length != 1, generator, ...) */
    DBG_REQ(request, "WSGI iterable is some other iterable");
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
    Py_XDECREF(first_chunk);
    return false;
  }

  /* Special-case HTTP 204 and 304 */
  if (!strncmp(_PEP3333_Bytes_AS_DATA(request->status), "204", 3) ||
      !strncmp(_PEP3333_Bytes_AS_DATA(request->status), "304", 3)) {
    request->state.response_length_unknown = false;
  }
  
  /* keep-alive cruft */
  if(http_should_keep_alive(&request->parser.parser)) { 
    if(request->state.response_length_unknown) {
      if(request->parser.parser.http_major > 0 && request->parser.parser.http_minor > 0) {
        /* On HTTP 1.1, we can use Transfer-Encoding: chunked. */
        DBG_REQ(request, "Content-Length unknown, HTTP/1.1 -> Connection: will keep-alive with chunked response");
        request->state.chunked_response = true;
        request->state.keep_alive = true;
      } else {
        /* On HTTP 1.0, we can only resort to closing the connection.  */
        DBG_REQ(request, "Content-Length unknown, HTTP/1.10 -> will close");
        request->state.keep_alive = false;
      }
    } else {
      /* We know the content-length. Can always keep-alive. */
        DBG_REQ(request, "Content-Length known -> will keep alive");
      request->state.keep_alive = true;
    }
  } else {
    /* Explicit "Connection: close" (HTTP 1.1) or missing "Connection: keep-alive" (HTTP 1.0) */
    DBG_REQ(request, "Connection: close request by client");
    request->state.keep_alive = false;
  }

  /* Get the headers and concatenate the first body chunk.
   * In the first place this makes the code more simple because afterwards
   * we can throw away the first chunk PyObject; but it also is an optimization:
   * At least for small responses, the complete response could be sent with
   * one send() call (in server.c:ev_io_on_write) which is a (tiny) performance
   * booster because less kernel calls means less kernel call overhead. */
  Py_ssize_t length;
  PyObject* buf;
  wsgi_getheaders(request, &buf, &length);

  if(first_chunk == NULL) {
    _PEP3333_Bytes_Resize(&buf, length);
    goto out;
  }

  if(request->state.chunked_response) {
    PyObject* new_chunk = wrap_http_chunk_cruft_around(first_chunk);
    Py_DECREF(first_chunk);
    assert(_PEP3333_Bytes_GET_SIZE(new_chunk) >= _PEP3333_Bytes_GET_SIZE(first_chunk) + 5);
    first_chunk = new_chunk;
  }

  _PEP3333_Bytes_Resize(&buf, length + _PEP3333_Bytes_GET_SIZE(first_chunk));
  memcpy((void *)(_PEP3333_Bytes_AS_DATA(buf)+length), _PEP3333_Bytes_AS_DATA(first_chunk),
         _PEP3333_Bytes_GET_SIZE(first_chunk));
  Py_DECREF(first_chunk);

out:
  request->state.wsgi_call_done = true;
  request->current_chunk = buf;
  request->current_chunk_p = 0;
  return true;
}

static inline PyObject*
clean_headers(PyObject* headers, bool* found_content_length)
{

  if(!PyList_Check(headers)) {
    TYPE_ERROR("start response argument 2", "a list of 2-tuples", headers);
    return NULL;
  }

  for(Py_ssize_t i=0; i<PyList_GET_SIZE(headers); ++i) {
    PyObject* tuple = PyList_GET_ITEM(headers, i);
    if(!PyTuple_Check(tuple) || PyTuple_GET_SIZE(tuple) != 2) {
      TYPE_ERROR_INNER("start_response argument 2", "a list of 2-tuples (field: str, value: str)",
        "(found invalid '%.200s' object at position %zd)", Py_TYPE(tuple)->tp_name, i);
      return NULL;
    }
  }

  /* Create copy of headers list that we may modify */
  PyObject* new_headers = PyList_New(PyList_GET_SIZE(headers));
  if (new_headers == NULL) {
    return NULL;
  }

  for(Py_ssize_t i=0; i<PyList_GET_SIZE(headers); ++i) {
    PyObject* tuple = PyList_GET_ITEM(headers, i);

    PyObject* unicode_field = PyTuple_GET_ITEM(tuple, 0);
    PyObject* unicode_value = PyTuple_GET_ITEM(tuple, 1);

    PyObject* bytes_field = _PEP3333_BytesLatin1_FromUnicode(unicode_field);
    PyObject* bytes_value = _PEP3333_BytesLatin1_FromUnicode(unicode_value);

    if (bytes_field == NULL || bytes_value == NULL) {
      TYPE_ERROR_INNER("start_response argument 2", "a list of 2-tuples (field: str, value: str)",
        "(found invalid ('%.200s', '%.200s') tuple at position %zd)", Py_TYPE(unicode_field)->tp_name, Py_TYPE(unicode_value)->tp_name, i);
      Py_DECREF(new_headers);
      Py_XDECREF(bytes_field);
      Py_XDECREF(bytes_value);
      return NULL;
    }

    PyList_SET_ITEM(new_headers, i, PyTuple_Pack(2, bytes_field, bytes_value));

    if(!strncasecmp(_PEP3333_Bytes_AS_DATA(bytes_field), "Content-Length", _PEP3333_Bytes_GET_SIZE(bytes_field)))
      *found_content_length = true;

    Py_DECREF(bytes_field);
    Py_DECREF(bytes_value);
  }
  return new_headers;
}


static void
wsgi_getheaders(Request* request, PyObject** buf, Py_ssize_t *length)
{
  Py_ssize_t length_upperbound = strlen("HTTP/1.1 ") + _PEP3333_Bytes_GET_SIZE(request->status) + strlen("\r\nConnection: Keep-Alive") + strlen("\r\nTransfer-Encoding: chunked") + strlen("\r\n\r\n");
  for(Py_ssize_t i=0; i<PyList_GET_SIZE(request->headers); ++i) {
    PyObject* tuple = PyList_GET_ITEM(request->headers, i);
    PyObject* field = PyTuple_GET_ITEM(tuple, 0);
    PyObject* value = PyTuple_GET_ITEM(tuple, 1);
    length_upperbound += strlen("\r\n") + _PEP3333_Bytes_GET_SIZE(field) + strlen(": ") + _PEP3333_Bytes_GET_SIZE(value);
  }

  PyObject* bufobj = _PEP3333_Bytes_FromStringAndSize(NULL, length_upperbound);
  char* bufp = (char *)_PEP3333_Bytes_AS_DATA(bufobj);

  #define buf_write(src, len) \
    do { \
      size_t n = len; \
      const char* s = src;  \
      while(n--) *bufp++ = *s++; \
    } while(0)
  #define buf_write2(src) buf_write(src, strlen(src))

  /* First line, e.g. "HTTP/1.1 200 Ok" */
  buf_write2("HTTP/1.1 ");
  buf_write(_PEP3333_Bytes_AS_DATA(request->status),
            _PEP3333_Bytes_GET_SIZE(request->status));

  /* Headers, from the `request->headers` mapping.
   * [("Header1", "value1"), ("Header2", "value2")]
   * --> "Header1: value1\r\nHeader2: value2"
   */
  for(Py_ssize_t i=0; i<PyList_GET_SIZE(request->headers); ++i) {
    PyObject* tuple = PyList_GET_ITEM(request->headers, i);
    PyObject* field = PyTuple_GET_ITEM(tuple, 0);
    PyObject* value = PyTuple_GET_ITEM(tuple, 1);
    buf_write2("\r\n");
    buf_write(_PEP3333_Bytes_AS_DATA(field), _PEP3333_Bytes_GET_SIZE(field));
    buf_write2(": ");
    buf_write(_PEP3333_Bytes_AS_DATA(value), _PEP3333_Bytes_GET_SIZE(value));
  }

  /* See `wsgi_call_application` */
  if(request->state.keep_alive) {
    buf_write2("\r\nConnection: Keep-Alive");
    if(request->state.chunked_response) {
      buf_write2("\r\nTransfer-Encoding: chunked");
    }
  } else {
    buf_write2("\r\nConnection: close");
  }

  buf_write2("\r\n\r\n");

  *buf = bufobj;
  *length = bufp - _PEP3333_Bytes_AS_DATA(bufobj);
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
    if(!_PEP3333_Bytes_Check(next)) {
      TYPE_ERROR("wsgi iterable items", "bytes", next);
      Py_DECREF(next);
      return NULL;
    }
    if(_PEP3333_Bytes_GET_SIZE(next))
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
    request->state.response_length_unknown = true;
  }

  PyObject* exc_info = NULL;
  PyObject* status_unicode = NULL;
  PyObject* headers = NULL;
  if(!PyArg_UnpackTuple(args, "start_response", 2, 3, &status_unicode, &headers, &exc_info))
    return NULL;

  if(exc_info && exc_info != Py_None) {
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

  request->status = _PEP3333_BytesLatin1_FromUnicode(status_unicode);
  if (request->status == NULL) {
    return NULL;
  } else if (_PEP3333_Bytes_GET_SIZE(request->status) < 3) {
    PyErr_SetString(PyExc_ValueError, "'status' must be 3-digit");
    Py_CLEAR(request->status);
    return NULL;
  }


  bool found_content_length = false;
  request->headers = clean_headers(headers, &found_content_length);
  if (request->headers == NULL) {
    return NULL;
  }

  request->state.response_length_unknown = !found_content_length;
  request->state.start_response_called = true;

  Py_RETURN_NONE;
}

PyTypeObject StartResponse_Type = {
  PyVarObject_HEAD_INIT(NULL, 0)
  "start_response",           /* tp_name (__name__)                         */
  sizeof(StartResponse),      /* tp_basicsize                               */
  0,                          /* tp_itemsize                                */
  (destructor)PyObject_FREE,  /* tp_dealloc                                 */
  0, 0, 0, 0, 0, 0, 0, 0, 0,  /* tp_{print,getattr,setattr,compare,...}     */
  start_response              /* tp_call (__call__)                         */
};


PyObject*
wrap_http_chunk_cruft_around(PyObject* chunk)
{
  /* Who the hell decided to use decimal representation for Content-Length
   * but hexadecimal representation for chunk lengths btw!?! Fuck W3C */
  size_t chunklen = _PEP3333_Bytes_GET_SIZE(chunk);
  assert(chunklen);
  char buf[strlen("ffffffff") + 2];
  size_t n = sprintf(buf, "%x\r\n", (unsigned int)chunklen);
  PyObject* new_chunk = _PEP3333_Bytes_FromStringAndSize(NULL, n + chunklen + 2);
  char * new_chunk_p = (char *)_PEP3333_Bytes_AS_DATA(new_chunk);
  memcpy(new_chunk_p, buf, n);
  new_chunk_p += n;
  memcpy(new_chunk_p, _PEP3333_Bytes_AS_DATA(chunk), chunklen);
  new_chunk_p += chunklen;
  *new_chunk_p++ = '\r'; *new_chunk_p = '\n';
  assert(new_chunk_p == _PEP3333_Bytes_AS_DATA(new_chunk) + n + chunklen + 1);
  return new_chunk;
}
