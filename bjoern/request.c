#include <Python.h>
#include "request.h"
#include "filewrapper.h"

#include "py2py3.h"

static inline void PyDict_ReplaceKey(PyObject* dict, PyObject* k1, PyObject* k2);
static http_parser_settings parser_settings;
static PyObject* wsgi_base_dict = NULL;

static PyObject *IO_module;

#define _free_and_unset_if_set(v) \
  do { \
    Py_XDECREF(v); \
    v = NULL; \
  } while(0)

Request* Request_new(ServerInfo* server_info, int client_fd, const char* client_addr)
{
  Request* request = malloc(sizeof(Request));
#ifdef DEBUG
  static unsigned long request_id = 0;
  request->id = request_id++;
#endif
  request->server_info = server_info;
  request->client_fd = client_fd;
  request->client_addr = _Unicode_FromString(client_addr);
  http_parser_init((http_parser*)&request->parser, HTTP_REQUEST);
  request->parser.parser.data = request;
  request->parser.field = NULL;
  Request_reset(request);
  return request;
}

void Request_reset(Request* request)
{
  memset(&request->state, 0, sizeof(Request) - (size_t)&((Request*)NULL)->state);
  request->state.response_length_unknown = true;
  request->parser.last_was_data = true;
  request->parser.invalid_header = false;
  _free_and_unset_if_set(request->parser.field);
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
  _free_and_unset_if_set(request->parser.field);
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

#define _set_header(k, v) PyDict_SetItem(REQUEST->headers, k, v);
  /* PyDict_SetItem() increases the ref-count for value */
#define _set_header_free_value(k, v) \
  do { \
    PyObject* val = (v); \
    _set_header(k, val); \
    Py_DECREF(val); \
  } while(0)

#define _set_or_append_header(k, val, len) \
  do { \
    PyObject *py_val = _Unicode_FromStringAndSize(val, len); \
    PyObject *py_val_old = PyDict_GetItem(REQUEST->headers, k); \
    \
    if(py_val_old) { \
      PyObject *py_val_new = _Unicode_Concat(py_val_old, py_val); \
      PyDict_SetItem(REQUEST->headers, k, py_val_new); \
      Py_DECREF(py_val_new); \
    } else { \
      PyDict_SetItem(REQUEST->headers, k, py_val); \
    } \
    Py_DECREF(py_val); \
  } while(0)

static int
on_message_begin(http_parser* parser)
{
  REQUEST->headers = PyDict_New();
  PARSER->field = NULL;
  return 0;
}

static int
on_path(http_parser* parser, const char* path, size_t len)
{
  if(!(len = unquote_url_inplace((char*)path, len)))
    return 1;
  _set_or_append_header(_PATH_INFO, path, len);
  return 0;
}

static int
on_query_string(http_parser* parser, const char* query, size_t len)
{
  _set_or_append_header(_QUERY_STRING, query, len);
  return 0;
}

static int
on_header_field(http_parser* parser, const char* field, size_t len)
{
  if(PARSER->last_was_data) {
    _free_and_unset_if_set(PARSER->field);
    PARSER->field = _Unicode_FromStringAndSize("HTTP_", 5);
    PARSER->last_was_data = false;
    PARSER->invalid_header = false;
  }

  if(PARSER->invalid_header) {
    return 0;
  }

  char field_processed[len];
  for(size_t i=0; i<len; i++) {
    char c = field[i];
    if(c == '_') {
      // CVE-2015-0219
      PARSER->invalid_header = true;
      return 0;
    } else if (c == '-') {
      field_processed[i] = '_';
    } else if(c >= 'a' && c <= 'z') {
      field_processed[i] = c - ('a'-'A');
    } else {
      field_processed[i] = c;
    }
  }

  PyObject *field_old = PARSER->field;
  PyObject *field_new = _Unicode_FromStringAndSize(field_processed, len);
  PARSER->field = _Unicode_Concat(field_old, field_new);
  Py_DECREF(field_old);
  Py_DECREF(field_new);

  return 0;
}

static int
on_header_value(http_parser* parser, const char* value, size_t len)
{
  PARSER->last_was_data = true;
  if(!PARSER->invalid_header) {
    _set_or_append_header(PARSER->field, value, len);
  }
  return 0;
}

static int
on_headers_complete(http_parser* parser)
{
  _free_and_unset_if_set(PARSER->field);
  return 0;
}

static int
on_body(http_parser* parser, const char* data, const size_t len)
{
  PyObject *body;

  body = PyDict_GetItem(REQUEST->headers, _wsgi_input);
  if (body == NULL) {
    if(!parser->content_length) {
      REQUEST->state.error_code = HTTP_LENGTH_REQUIRED;
      return 1;
    }
    body = PyObject_CallMethodObjArgs(IO_module, _BytesIO, NULL);
    if (body == NULL) {
      return 1;
    }
    _set_header_free_value(_wsgi_input, body);
  }
  PyObject *temp_data = _Bytes_FromStringAndSize(data, len);
  PyObject *tmp = PyObject_CallMethodObjArgs(body, _write, temp_data, NULL);
  Py_DECREF(tmp); /* Never throw away return objects from py-api */
  Py_DECREF(temp_data);
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
      _Unicode_FromString(http_method_str(parser->method))
      );
  }

  /* REMOTE_ADDR */
  _set_header(_REMOTE_ADDR, REQUEST->client_addr);

  PyObject* body = PyDict_GetItem(REQUEST->headers, _wsgi_input);
  if(body) {
    /* first do a seek(0) and then read() returns all data */
    PyObject *buf = PyObject_CallMethodObjArgs(body, _seek, _FromLong(0), NULL);
    Py_DECREF(buf); /* Discard the return value */
  } else {
    /* Request has no body */
    body = PyObject_CallMethodObjArgs(IO_module, _BytesIO, NULL);
    _set_header_free_value(_wsgi_input, body);
  }

  PyDict_Update(REQUEST->headers, wsgi_base_dict);

  REQUEST->state.parse_finished = true;
  return 0;
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

void _initialize_request_module(ServerInfo* server_info)
{
  IO_module = PyImport_ImportModule("io");
  if (IO_module == NULL) {
    /* PyImport_ImportModule should have exception set already */
    return;
  }

  if(wsgi_base_dict == NULL) {
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
      PyTuple_Pack(2, _FromLong(1), _FromLong(0))
    );

    /* dct['wsgi.url_scheme'] = 'http'
     * (This can be hard-coded as there is no TLS support in bjoern.) */
    PyDict_SetItemString(
      wsgi_base_dict,
      "wsgi.url_scheme",
      _Unicode_FromString("http")
    );

    /* dct['wsgi.errors'] = sys.stderr */
    PyDict_SetItemString(
      wsgi_base_dict,
      "wsgi.errors",
      PySys_GetObject("stderr")
    );

    /* dct['wsgi.multithread'] = False
     * (Tell the application that it is being run
     *  in a single-threaded environment.) */
    PyDict_SetItemString(
      wsgi_base_dict,
      "wsgi.multithread",
      Py_False
    );

    /* dct['wsgi.multiprocess'] = True
     * (Tell the application that it is being run
     *  in a multi-process environment.) */
    PyDict_SetItemString(
      wsgi_base_dict,
      "wsgi.multiprocess",
      Py_True
    );

    /* dct['wsgi.run_once'] = False
     * (bjoern is no CGI gateway) */
    PyDict_SetItemString(
      wsgi_base_dict,
      "wsgi.run_once",
      Py_False
    );

    /* dct['SERVER_NAME'] = '...'
     * dct['SERVER_PORT'] = '...'
     * Both are required by WSGI specs. */
    if (server_info->host) {
      PyDict_SetItemString(wsgi_base_dict, "SERVER_NAME", server_info->host);

      if (server_info->port == Py_None) {
      PyDict_SetItemString(wsgi_base_dict, "SERVER_PORT", PyUnicode_FromFormat(""));
      } else {
        PyDict_SetItemString(wsgi_base_dict, "SERVER_PORT", PyUnicode_FromFormat("%i", server_info->port));
      }
     } else {
      /* SERVER_NAME is required, but not usefull with UNIX type sockets */
      PyDict_SetItemString(wsgi_base_dict, "SERVER_NAME", PyUnicode_FromFormat(""));
      PyDict_SetItemString(wsgi_base_dict, "SERVER_PORT", PyUnicode_FromFormat(""));
    }
  }
}
