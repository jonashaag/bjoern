#include "request.h"

#define REQUEST_PREALLOC_N 100
static Request* _Request_from_prealloc();
static Request _preallocd[REQUEST_PREALLOC_N];
static char _preallocd_used[REQUEST_PREALLOC_N];

static http_parser_settings parser_settings;

static PyObject* wsgi_header(const char*, const size_t);


Request* Request_new(int client_fd)
{
    Request* req = _Request_from_prealloc();
    if(req == NULL)
        req = malloc(sizeof(Request));
    if(req == NULL)
        return NULL;

    req->client_fd = client_fd;
    req->headers = NULL;
    req->state = REQUEST_FRESH;
    http_parser_init((http_parser*)&req->parser, HTTP_REQUEST);
    req->parser.parser.data = req;

    return req;
}

void Request_parse(Request* request,
                   const char* data,
                   const size_t data_len) {
    if(data_len)
        http_parser_execute((http_parser*)&request->parser,
                            &parser_settings, data, data_len);
    else
        request->state = REQUEST_PARSE_ERROR;
}

void Request_free(Request* req)
{
    if(req >= _preallocd &&
       req <= _preallocd+REQUEST_PREALLOC_N*sizeof(Request)) {
        _preallocd_used[req-_preallocd] = false;
    } else {
        free(req);
    }
}

Request* _Request_from_prealloc()
{
    static int i = -1;

    if(i < 0) {
        memset(_preallocd_used, 0, sizeof(char)*REQUEST_PREALLOC_N);
        i = 0;
    }

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
#define UPDATE_LEN(name) \
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
    PyDict_SetItemString(
        REQUEST->headers,
        "PATH_INFO",
        wsgi_header(path_start, path_len)
    );
    return 0;
}

static int on_query_string(http_parser* parser,
                           const char* query_start,
                           const size_t query_len) {
    PyDict_SetItemString(
        REQUEST->headers,
        "QUERY_STRING",
        wsgi_header(query_start, query_len)
    );
    return 0;
}

static int on_url(http_parser* parser,
                  const char* url_start,
                  const size_t url_len) {
    PyDict_SetItemString(
        REQUEST->headers,
        "REQUEST_URI",
        wsgi_header(url_start, url_len)
    );
    return 0;
}

static int on_fragment(http_parser* parser,
                       const char* fragm_start,
                       const size_t fragm_len) {
    PyDict_SetItemString(
        REQUEST->headers,
        "HTTP_FRAGMENT",
        wsgi_header(fragm_start, fragm_len)
    );
    return 0;
}

static int on_header_field(http_parser* parser,
                           const char* field_start,
                           const size_t field_len) {
    if(PARSER->value_start) {
        /* Store previous header and start a new one */
        PyDict_SetItem(
            REQUEST->headers,
            wsgi_header(PARSER->field_start, PARSER->field_len),
            PyString_FromStringAndSize(PARSER->value_start, PARSER->value_len)
        );
        PARSER->field_start = field_start;
        PARSER->field_len = field_len;
        PARSER->value_start = NULL;
        PARSER->value_len = 0;

    } else if(PARSER->field_start) {
        UPDATE_LEN(field);
    }
    return 0;
}

static int on_header_value(http_parser* parser,
                           const char* value_start,
                           const size_t value_len) {
    if(PARSER->value_start)
        UPDATE_LEN(value);
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
    return 0;
}

static int on_body(http_parser* parser,
                   const char* body_start,
                   const size_t body_len) {
    return 0;
}

static int
on_message_complete(http_parser* parser)
{
    REQUEST->state = REQUEST_PARSE_DONE;
    return 0;
}


static PyObject*
wsgi_header(const char* data, size_t len)
{
    PyObject* obj = PyString_FromStringAndSize(/* empty string */ NULL,
                                               len+strlen("HTTP_")+1);
    char* dest = PyString_AS_STRING(obj);
    memcpy(dest, "HTTP_", 5);
    for(size_t i=0; i<len; ++i) {
        char c = *data;
        if(c == '-')
            *dest++ = '_';
        else if(c >= 'a' && c <= 'z')
            *dest++ = c - ('a' - 'A');
        else
            *dest++ = c;
    }
    *dest++ = '\0';
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
