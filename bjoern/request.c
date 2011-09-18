#include <Python.h>
#include <cStringIO.h>
#include "request.h"
#include "filewrapper.h"

static inline void PyDict_ReplaceKey(PyObject* dict, PyObject* k1, PyObject* k2);
static PyObject* wsgi_http_header(string header);
static http_parser_settings parser_settings;
static PyObject* wsgi_base_dict = NULL;

/* Non-public type from cStringIO I abuse in on_body */
typedef struct {
  PyObject_HEAD
  char *buf;
  Py_ssize_t pos, string_size;
  PyObject *pbuf;
} Iobject;

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
  request->parser.body = (string){NULL, 0};
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
   * A = XXX- PARSER->XXX.data
   * B = len
   * A + B = old header start to new header end
   */ \
  do { PARSER->name.len = (name - PARSER->name.data) + len; } while(0)

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
  PARSER->field = (string){NULL, 0};
  PARSER->value = (string){NULL, 0};
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
  if(PARSER->value.data) {
    /* Store previous header and start a new one */
    _set_header_free_both(
      wsgi_http_header(PARSER->field),
      PyString_FromStringAndSize(PARSER->value.data, PARSER->value.len)
    );
  } else if(PARSER->field.data) {
    UPDATE_LENGTH(field);
    return 0;
  }
  PARSER->field = (string){(char*)field, len};
  PARSER->value = (string){NULL, 0};
  return 0;
}

static int
on_header_value(http_parser* parser, const char* value, size_t len)
{
  if(PARSER->value.data) {
    UPDATE_LENGTH(value);
  } else {
    /* Start a new value */
    PARSER->value = (string){(char*)value, len};
  }
  return 0;
}

static int
on_headers_complete(http_parser* parser)
{
  if(PARSER->field.data) {
    _set_header_free_both(
      wsgi_http_header(PARSER->field),
      PyString_FromStringAndSize(PARSER->value.data, PARSER->value.len)
    );
  }
  return 0;
}

static int
on_body(http_parser* parser, const char* data, const size_t len)
{
  Iobject* body;

  body = (Iobject*)PyDict_GetItem(REQUEST->headers, _wsgi_input);
  if(body == NULL) {
    if(!parser->content_length) {
      REQUEST->state.error_code = HTTP_LENGTH_REQUIRED;
      return 1;
    }
    PyObject* buf = PyString_FromStringAndSize(NULL, parser->content_length);
    body = (Iobject*)PycStringIO->NewInput(buf);
    Py_XDECREF(buf);
    if(body == NULL)
      return 1;
    _set_header(_wsgi_input, (PyObject*)body);
    Py_DECREF(body);
  }
  memcpy(body->buf + body->pos, data, len);
  body->pos += len;
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

  PyObject* body = PyDict_GetItem(REQUEST->headers, _wsgi_input);
  if(body) {
    /* We abused the `pos` member for tracking the amount of data copied from
     * the buffer in on_body, so reset it to zero here. */
    ((Iobject*)body)->pos = 0;
  } else {
    /* Request has no body */
    _set_header_free_value(_wsgi_input, PycStringIO->NewInput(_empty_string));
  }

  PyDict_Update(REQUEST->headers, wsgi_base_dict);

  REQUEST->state.parse_finished = true;
  return 0;
}


static PyObject*
wsgi_http_header(string header)
{
  PyObject* obj = PyString_FromStringAndSize(NULL, header.len+strlen("HTTP_"));
  char* dest = PyString_AS_STRING(obj);

  *dest++ = 'H';
  *dest++ = 'T';
  *dest++ = 'T';
  *dest++ = 'P';
  *dest++ = '_';

  while(header.len--) {
    char c = *header.data++;
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

    /* dct['wsgi.file_wrapper'] = FileWrapper */
    PyDict_SetItemString(
      wsgi_base_dict,
      "wsgi.file_wrapper",
      (PyObject*)&FileWrapper_Type
    );

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
      _wsgi_version
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
