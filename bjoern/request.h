#ifndef __request_h__
#define __request_h__

#include <ev.h>
#include "http_parser.h"
#include "common.h"

void _request_module_initialize(const char* host, const int port);

typedef enum {
    REQUEST_FRESH                   = 1<<10,
    REQUEST_READING                 = 1<<11,
    REQUEST_PARSE_ERROR             = 1<<12,
    REQUEST_PARSE_DONE              = 1<<13,
    REQUEST_START_RESPONSE_CALLED   = 1<<14,
    REQUEST_RESPONSE_STATIC         = 1<<15,
    REQUEST_RESPONSE_HEADERS_SENT   = 1<<16,
    REQUEST_RESPONSE_WSGI           = 1<<17,
    REQUEST_WSGI_STRING_RESPONSE    = 1<<18,
    REQUEST_WSGI_FILE_RESPONSE      = 1<<19,
    REQUEST_WSGI_ITER_RESPONSE      = 1<<20
} request_state;

typedef struct {
    http_parser parser;
    const char* field_start;
    size_t field_len;
    const char* value_start;
    size_t value_len;
} bj_parser;

typedef struct {
#ifdef DEBUG
    unsigned long id;
#endif
    request_state state;
    int client_fd;
    ev_io ev_watcher;

    bj_parser parser;
    PyObject* headers; /* rm. */
    PyObject* body;

    void* response;
    PyObject* status;
    PyObject* iterable_next;
} Request;

Request* Request_new(int client_fd);
void Request_parse(Request*, const char*, const size_t);
void Request_free(Request*);


static PyObject
    * _PATH_INFO,
    * _QUERY_STRING,
    * _REQUEST_URI,
    * _HTTP_FRAGMENT,
    * _REQUEST_METHOD,
    * _wsgi_input,
    * _SERVER_PROTOCOL,
    * _GET,
    * _POST,
    * _CONTENT_LENGTH,
    * _CONTENT_TYPE,
    * _HTTP_1_1,
    * _HTTP_1_0,
    * _empty_string
;

#endif
