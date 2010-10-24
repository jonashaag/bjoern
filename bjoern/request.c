#include <Python.h>
#include <cStringIO.h>
#include "request.h"

#define REQUEST_PREALLOC_N 100
static Request* _Request_from_prealloc();
static Request _preallocd[REQUEST_PREALLOC_N];
static char _preallocd_used[REQUEST_PREALLOC_N];

static PyObject* wsgi_http_header(const char*, const size_t);
static http_parser_settings parser_settings;
static PyObject* wsgi_base_dict;


Request* Request_new(int client_fd)
{
    Request* req = _Request_from_prealloc();
    if(req == NULL)
        req = malloc(sizeof(Request));
    if(req == NULL)
        return NULL;

#ifdef DEBUG
    static unsigned long req_id = 0;
    req->id = req_id++;
#endif

    req->client_fd = client_fd;
    req->state = REQUEST_FRESH;
    http_parser_init((http_parser*)&req->parser, HTTP_REQUEST);
    req->parser.parser.data = req;

    req->headers = NULL;
    req->body = NULL;
    req->response = NULL;
    req->status = NULL;

    return req;
}

void Request_parse(Request* request,
                   const char* data,
                   const size_t data_len) {
    if(data_len) {
        size_t nparsed = http_parser_execute((http_parser*)&request->parser,
                                             &parser_settings, data, data_len);
        if(nparsed == data_len)
            /* everything fine */
            return;
    }

    request->state = REQUEST_PARSE_ERROR | HTTP_BAD_REQUEST;
}

void Request_free(Request* req)
{
    if(req->state & REQUEST_RESPONSE_WSGI) {
        Py_XDECREF(req->response);
    }

#if 0
    else
        free(req->response);
#endif

    Py_XDECREF(req->body);
    if(req->headers)
        assert(req->headers->ob_refcnt >= 1);
    if(req->status)
        assert(req->status->ob_refcnt >= 1);
    Py_XDECREF(req->headers);
    Py_XDECREF(req->status);

    if(req >= _preallocd &&
       req <= _preallocd+REQUEST_PREALLOC_N*sizeof(Request)) {
        _preallocd_used[req-_preallocd] = false;
    } else {
        free(req);
    }

#if 0
    DBG_REFCOUNT_REQ(req, _PATH_INFO);
    DBG_REFCOUNT_REQ(req, _QUERY_STRING);
    DBG_REFCOUNT_REQ(req, _REQUEST_URI);
    DBG_REFCOUNT_REQ(req, _HTTP_FRAGMENT);
    DBG_REFCOUNT_REQ(req, _REQUEST_METHOD);
    DBG_REFCOUNT_REQ(req, _wsgi_input);
    DBG_REFCOUNT_REQ(req, _SERVER_PROTOCOL);
    DBG_REFCOUNT_REQ(req, _GET);
    DBG_REFCOUNT_REQ(req, _POST);
    DBG_REFCOUNT_REQ(req, _CONTENT_LENGTH);
    DBG_REFCOUNT_REQ(req, _CONTENT_TYPE);
    DBG_REFCOUNT_REQ(req, _HTTP_1_1);
    DBG_REFCOUNT_REQ(req, _HTTP_1_0);
    DBG_REFCOUNT_REQ(req, _empty_string);
#endif
}

Request* _Request_from_prealloc()
{
    static int i = 0;
    for(; i<REQUEST_PREALLOC_N; ++i) {
        if(!_preallocd_used[i]) {
            _preallocd_used[i] = true;
            return &_preallocd[i];
        }
    }
    i = 0;
    return NULL;
}


/*
 * PARSER CALLBACKS
 */

#define REQUEST ((Request*)parser->data)
#define PARSER  ((bj_parser*)parser)
#define _update_length(name) \
    /* Update the len of a header field/value.

       Short explaination of the pointer arithmetics fun used here:

         [old header data ] ...stuff... [ new header data ]
         ^-------------- A -------------^--------B--------^

       A = XXX_start - PARSER->XXX_start
       B = XXX_len
       A + B = old header start to new header end
    */ \
    do {\
        PARSER->name##_len = (name##_start - PARSER->name##_start) \
                                + name##_len; \
    } while(0)

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

static int
on_message_begin(http_parser* parser)
{
    REQUEST->headers = PyDict_New();
    PARSER->field_start = NULL;
    PARSER->field_len = 0;
    PARSER->value_start = NULL;
    PARSER->value_len = 0;
    return 0;
}

static int on_path(http_parser* parser,
                   const char* path_start,
                   const size_t path_len) {
    _set_header_free_value(
        _PATH_INFO,
        PyString_FromStringAndSize(path_start, path_len)
    );
    return 0;
}

static int on_query_string(http_parser* parser,
                           const char* query_start,
                           const size_t query_len) {
    _set_header_free_value(
        _QUERY_STRING,
        PyString_FromStringAndSize(query_start, query_len)
    );
    return 0;
}

static int on_url(http_parser* parser,
                  const char* url_start,
                  const size_t url_len) {
    _set_header_free_value(
        _REQUEST_URI,
        PyString_FromStringAndSize(url_start, url_len)
    );
    return 0;
}

static int on_fragment(http_parser* parser,
                       const char* fragm_start,
                       const size_t fragm_len) {
    _set_header_free_value(
        _HTTP_FRAGMENT,
        PyString_FromStringAndSize(fragm_start, fragm_len)
    );
    return 0;
}

static int on_header_field(http_parser* parser,
                           const char* field_start,
                           const size_t field_len) {
    if(PARSER->value_start) {
        /* Store previous header and start a new one */
        _set_header_free_both(
            wsgi_http_header(PARSER->field_start, PARSER->field_len),
            PyString_FromStringAndSize(PARSER->value_start, PARSER->value_len)
        );

    } else if(PARSER->field_start) {
        _update_length(field);
        return 0;
    }

    PARSER->field_start = field_start;
    PARSER->field_len = field_len;
    PARSER->value_start = NULL;
    PARSER->value_len = 0;

    return 0;
}

static int on_header_value(http_parser* parser,
                           const char* value_start,
                           const size_t value_len) {
    if(PARSER->value_start)
        _update_length(value);
    else {
        /* Start a new value */
        PARSER->value_start = value_start;
        PARSER->value_len = value_len;
    }
    return 0;
}

static int
on_headers_complete(http_parser* parser)
{
    if(PARSER->field_start) {
        _set_header_free_both(
            wsgi_http_header(PARSER->field_start, PARSER->field_len),
            PyString_FromStringAndSize(PARSER->value_start, PARSER->value_len)
        );
    }
    return 0;
}

static int on_body(http_parser* parser,
                   const char* body_start,
                   const size_t body_len) {
    if(!REQUEST->body) {
        if(!parser->content_length) {
            REQUEST->state = REQUEST_PARSE_ERROR | HTTP_LENGTH_REQUIRED;
            return 1;
        }
        REQUEST->body = PycStringIO->NewOutput(parser->content_length);
    }

    if(PycStringIO->cwrite(REQUEST->body, body_start, body_len) < 0) {
        REQUEST->state = REQUEST_PARSE_ERROR | HTTP_SERVER_ERROR;
        return 1;
    }

    return 0;
}

static int
on_message_complete(http_parser* parser)
{
    switch(parser->method) {
        case HTTP_GET:
            _set_header(_REQUEST_METHOD, _GET);
            break;
        case HTTP_POST:
            _set_header(_REQUEST_METHOD, _POST);
            break;
        default:
            _set_header_free_value(
                _REQUEST_METHOD,
                PyString_FromString(http_method_str(parser->method))
            );
    }

    _set_header_free_value(
        _wsgi_input,
        PycStringIO->NewInput(
            REQUEST->body ? PycStringIO->cgetvalue(REQUEST->body)
                          : _empty_string
        )
    );

    _set_header(
        _SERVER_PROTOCOL,
        parser->http_minor == 1 ? _HTTP_1_1 : _HTTP_1_0
    );

    PyDict_Update(REQUEST->headers, wsgi_base_dict);

    REQUEST->state |= REQUEST_PARSE_DONE;
    return 0;
}


/* Case insensitive string comparison */
static inline bool
string_iequal(const char* a, size_t len, const char* b)
{
    if(len != strlen(b))
        return false;
    for(size_t i=0; i<len; ++i)
        if(a[i] != b[i] && a[i] - ('a'-'A') != b[i])
            return false;
    return true;
}

static PyObject*
wsgi_http_header(const char* data, size_t len)
{
    if(string_iequal(data, len, "Content-Length")) {
        Py_INCREF(_CONTENT_LENGTH);
        return _CONTENT_LENGTH;
    }
    if(string_iequal(data, len, "Content-Type")) {
        Py_INCREF(_CONTENT_TYPE);
        return _CONTENT_TYPE;
    }

    PyObject* obj = PyString_FromStringAndSize(/* empty string */ NULL,
                                               len+strlen("HTTP_"));
    char* dest = PyString_AS_STRING(obj);

    *dest++ = 'H';
    *dest++ = 'T';
    *dest++ = 'T';
    *dest++ = 'P';
    *dest++ = '_';

    for(; len; --len, ++data, ++dest) {
        char c = *data;
        if(c == '-')
            *dest = '_';
        else if(c >= 'a' && c <= 'z')
            *dest = c - ('a'-'A');
        else
            *dest = c;
    }
    return obj;
}


static http_parser_settings
parser_settings = {
    .on_message_begin    = on_message_begin,
    .on_path             = on_path,
    .on_query_string     = on_query_string,
    .on_url              = on_url,
    .on_fragment         = on_fragment,
    .on_header_field     = on_header_field,
    .on_header_value     = on_header_value,
    .on_headers_complete = on_headers_complete,
    .on_body             = on_body,
    .on_message_complete = on_message_complete
};

void
_request_module_initialize(const char* server_host, const int server_port)
{
    memset(_preallocd_used, 0, sizeof(char)*REQUEST_PREALLOC_N);

    #define _(name) _##name = PyString_FromString(#name)
    _(PATH_INFO);
    _(QUERY_STRING);
    _(REQUEST_URI);
    _(HTTP_FRAGMENT);
    _(REQUEST_METHOD);
    _(SERVER_PROTOCOL);
    _(GET);
    _(POST);
    _(CONTENT_LENGTH);
    _(CONTENT_TYPE);
    _HTTP_1_1 = PyString_FromString("HTTP/1.1");
    _HTTP_1_0 = PyString_FromString("HTTP/1.0");
    _wsgi_input = PyString_FromString("wsgi.input");
    _empty_string = PyString_FromString("");

    PycString_IMPORT;

    wsgi_base_dict = PyDict_New();

    /* dct['wsgi.version'] = (1, 0) */
    PyDict_SetItemString(
        wsgi_base_dict,
        "wsgi.version",
        PyTuple_Pack(2, PyInt_FromLong(1), PyInt_FromLong(0))
    );

    /* dct['wsgi.url_scheme'] = 'http'
     * (This can be hard-coded as there is no TLS support in bjoern.)
     */
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
     * "Can the server be ran in a thread?"
     */
    PyDict_SetItemString(
        wsgi_base_dict,
        "wsgi.multithread",
        Py_True
    );

    /* dct['wsgi.multiprocess'] = True
     * ... and this one "Can the server process be forked?"
     */
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
