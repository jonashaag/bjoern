#include <Python.h>
#include <cStringIO.h>
#include "request.h"

static inline void PyDict_ReplaceKey(PyObject* dict, PyObject* k1, PyObject* k2);
static PyObject* wsgi_http_header(Request*, const char*, const size_t);
static http_parser_settings parser_settings;
static PyObject* wsgi_base_dict = NULL;

Request* Request_new(int client_fd, const char* client_addr)
{
  Request* request = malloc(sizeof(Request));
#ifdef DEBUG
  static unsigned long request_id = 0;
  request->id = request_id++;
#endif
  request->client_fd = client_fd;
  request->client_addr = PyString_FromString(client_addr);
  http_parser_init((http_parser*)&request->parser, HTTP_REQUEST);
  request->parser.parser.data = request;
  Request_reset(request);
  return request;
}

void Request_reset(Request* request)
{
  memset(&request->state, 0, sizeof(Request) - (size_t)&((Request*)NULL)->state);
  request->state.response_length_unknown = true;
}

void Request_free(Request* request)
{
  Request_clean(request);
  Py_DECREF(request->client_addr);
  free(request);
}

void Request_clean(Request* request)
{
  if(request->iterable) {
    /* Call 'iterable.close()' if available */
    PyObject* close_method = PyObject_GetAttr(request->iterable, _close);
    if(close_method == NULL) {
      if(PyErr_ExceptionMatches(PyExc_AttributeError))
        PyErr_Clear();
    } else {
      PyObject_CallObject(close_method, NULL);
      Py_DECREF(close_method);
    }
    if(PyErr_Occurred()) PyErr_Print();
    Py_DECREF(request->iterable);
  }
  Py_XDECREF(request->iterator);
  Py_XDECREF(request->body);
  if(request->headers)
    assert(request->headers->ob_refcnt >= 1);
  if(request->status)
    assert(request->status->ob_refcnt >= 1);
  Py_XDECREF(request->headers);
  Py_XDECREF(request->status);
}

/* Parse stuff */

void Request_parse(Request* request, const char* data, const size_t data_len)
{
  assert(data_len);
  size_t nparsed = http_parser_execute((http_parser*)&request->parser,
                                       &parser_settings, data, data_len);
  if(nparsed != data_len)
    request->state.error_code = HTTP_BAD_REQUEST;
}

#define REQUEST ((Request*)parser->data)
#define PARSER  ((bj_parser*)parser)
#define UPDATE_LENGTH(name) \
  /* Update the len of a header field/value.
   *
   * Short explaination of the pointer arithmetics fun used here:
   *
   *   [old header data ] ...stuff... [ new header data ]
   *   ^-------------- A -------------^--------B--------^
   *
   * A = XXX- PARSER->XXX_start
   * B = len
   * A + B = old header start to new header end
   */ \
  do { PARSER->name##_len = (name - PARSER->name##_start) + len; } while(0)

#define _set_header(k, v) PyDict_SetItem(REQUEST->headers, k, v);
#define _set_header_free_value(k, v) \
  do { \
    PyObject* val = (v); \
    _set_header(k, val); \
    Py_DECREF(val); \
  } while(0)
#define _set_header_free_both(k, v) \
  do { \
    PyObject* key = (k); \
    PyObject* val = (v); \
    _set_header(key, val); \
    Py_DECREF(key); \
    Py_DECREF(val); \
  } while(0)

static int on_message_begin(http_parser* parser)
{
  REQUEST->headers = PyDict_New();
  PARSER->field_start = NULL;
  PARSER->field_len = 0;
  PARSER->value_start = NULL;
  PARSER->value_len = 0;
  return 0;
}

static int on_path(http_parser* parser, char* path, size_t len)
{
  if(!(len = unquote_url_inplace(path, len)))
    return 1;
  _set_header_free_value(_PATH_INFO, PyString_FromStringAndSize(path, len));
  return 0;
}

static int on_query_string(http_parser* parser, const char* query, size_t len)
{
  _set_header_free_value(_QUERY_STRING, PyString_FromStringAndSize(query, len));
  return 0;
}

static int on_header_field(http_parser* parser, const char* field, size_t len)
{
  if(PARSER->value_start) {
    /* Store previous header and start a new one */
    _set_header_free_both(
      wsgi_http_header(REQUEST, PARSER->field_start, PARSER->field_len),
      PyString_FromStringAndSize(PARSER->value_start, PARSER->value_len)
    );
  } else if(PARSER->field_start) {
    UPDATE_LENGTH(field);
    return 0;
  }
  PARSER->field_start = field;
  PARSER->field_len = len;
  PARSER->value_start = NULL;
  PARSER->value_len = 0;
  return 0;
}

static int
on_header_value(http_parser* parser, const char* value, size_t len)
{
  if(PARSER->value_start) {
    UPDATE_LENGTH(value);
  } else {
    /* Start a new value */
    PARSER->value_start = value;
    PARSER->value_len = len;
  }
  return 0;
}

static int
on_headers_complete(http_parser* parser)
{
  if(PARSER->field_start) {
    _set_header_free_both(
      wsgi_http_header(REQUEST, PARSER->field_start, PARSER->field_len),
      PyString_FromStringAndSize(PARSER->value_start, PARSER->value_len)
    );
  }
  return 0;
}

static int
on_body(http_parser* parser, const char* body, const size_t len)
{
  if(!REQUEST->body) {
    if(!parser->content_length) {
      REQUEST->state.error_code = HTTP_LENGTH_REQUIRED;
      return 1;
    }
    REQUEST->body = PycStringIO->NewOutput(parser->content_length);
  }
  if(PycStringIO->cwrite(REQUEST->body, body, len) < 0) {
    REQUEST->state.error_code = HTTP_SERVER_ERROR;
    return 1;
  }
  return 0;
}

static int
on_message_complete(http_parser* parser)
{
  /* HTTP_CONTENT_{LENGTH,TYPE} -> CONTENT_{LENGTH,TYPE} */
  PyDict_ReplaceKey(REQUEST->headers, _HTTP_CONTENT_LENGTH, _CONTENT_LENGTH);
  PyDict_ReplaceKey(REQUEST->headers, _HTTP_CONTENT_TYPE, _CONTENT_TYPE);

  /* SERVER_PROTOCOL (REQUEST_PROTOCOL) */
  _set_header(_SERVER_PROTOCOL, parser->http_minor == 1 ? _HTTP_1_1 : _HTTP_1_0);

  /* REQUEST_METHOD */
  if(parser->method == HTTP_GET) {
    /* I love useless micro-optimizations. */
    _set_header(_REQUEST_METHOD, _GET);
  } else {
    _set_header_free_value(_REQUEST_METHOD,
      PyString_FromString(http_method_str(parser->method)));
  }

  /* REMOTE_ADDR */
  _set_header(_REMOTE_ADDR, REQUEST->client_addr);

  /* wsgi.input */
  _set_header_free_value(
    _wsgi_input,
    PycStringIO->NewInput(REQUEST->body ? PycStringIO->cgetvalue(REQUEST->body)
                                        : _empty_string)
  );

  PyDict_Update(REQUEST->headers, wsgi_base_dict);

  REQUEST->state.parse_finished = true;
  return 0;
}


static PyObject*
wsgi_http_header(Request* request, const char* data, size_t len)
{
  PyObject* obj = PyString_FromStringAndSize(NULL, len+strlen("HTTP_"));
  char* dest = PyString_AS_STRING(obj);

  *dest++ = 'H';
  *dest++ = 'T';
  *dest++ = 'T';
  *dest++ = 'P';
  *dest++ = '_';

  while(len--) {
    char c = *data++;
    if(c == '-')
      *dest++ = '_';
    else if(c >= 'a' && c <= 'z')
      *dest++ = c - ('a'-'A');
    else
      *dest++ = c;
  }

  return obj;
}

static inline void
PyDict_ReplaceKey(PyObject* dict, PyObject* old_key, PyObject* new_key)
{
  PyObject* value = PyDict_GetItem(dict, old_key);
  if(value) {
    Py_INCREF(value);
    PyDict_DelItem(dict, old_key);
    PyDict_SetItem(dict, new_key, value);
    Py_DECREF(value);
  }
}


static http_parser_settings
parser_settings = {
  on_message_begin, on_path, on_query_string, NULL, NULL, on_header_field,
  on_header_value, on_headers_complete, on_body, on_message_complete
};

void _initialize_request_module(const char* server_host, const int server_port)
{
  if(wsgi_base_dict == NULL) {
    PycString_IMPORT;
    wsgi_base_dict = PyDict_New();

    /* dct['SCRIPT_NAME'] = '' */
    PyDict_SetItemString(
      wsgi_base_dict,
      "SCRIPT_NAME",
      _empty_string
    );

    /* dct['wsgi.version'] = (1, 0) */
    PyDict_SetItemString(
      wsgi_base_dict,
      "wsgi.version",
      PyTuple_Pack(2, PyInt_FromLong(1), PyInt_FromLong(0))
    );

    /* dct['wsgi.url_scheme'] = 'http'
     * (This can be hard-coded as there is no TLS support in bjoern.) */
    PyDict_SetItemString(
      wsgi_base_dict,
      "wsgi.url_scheme",
      PyString_FromString("http")
    );

    /* dct['wsgi.errors'] = sys.stderr */
    PyDict_SetItemString(
      wsgi_base_dict,
      "wsgi.errors",
      PySys_GetObject("stderr")
    );

    /* dct['wsgi.multithread'] = True
     * If I correctly interpret the WSGI specs, this means
     * "Can the server be ran in a thread?" */
    PyDict_SetItemString(
      wsgi_base_dict,
      "wsgi.multithread",
      Py_True
    );

    /* dct['wsgi.multiprocess'] = True
     * ... and this one "Can the server process be forked?" */
    PyDict_SetItemString(
      wsgi_base_dict,
      "wsgi.multiprocess",
      Py_True
    );

    /* dct['wsgi.run_once'] = False (bjoern is no CGI gateway) */
    PyDict_SetItemString(
      wsgi_base_dict,
      "wsgi.run_once",
      Py_False
    );
  }

  PyDict_SetItemString(
    wsgi_base_dict,
    "SERVER_NAME",
    PyString_FromString(server_host)
  );

  PyDict_SetItemString(
    wsgi_base_dict,
    "SERVER_PORT",
    PyString_FromFormat("%d", server_port)
  );
}
