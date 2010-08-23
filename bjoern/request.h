#ifndef __request_h__
#define __request_h__

#include <ev.h>
#include "http_parser.h"
#include "common.h"

void _request_module_initialize(const char* host, const int port);

typedef enum {
    REQUEST_FRESH                   = 1<<0,
    REQUEST_READING                 = 1<<1,
    REQUEST_PARSE_ERROR             = 1<<2,
    REQUEST_PARSE_DONE              = 1<<3,
    REQUEST_RESPONSE_STATIC         = 1<<4,
    REQUEST_RESPONSE_HEADERS_SENT   = 1<<5,
    REQUEST_RESPONSE_WSGI           = 1<<6,
    REQUEST_WSGI_STRING_RESPONSE    = 1<<7,
    REQUEST_WSGI_FILE_RESPONSE      = 1<<8,
    REQUEST_WSGI_ITER_RESPONSE      = 1<<9
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

    void* response;
    PyObject* response_headers;
    PyObject* status;
    PyObject* response_curiter;
} Request;

Request* Request_new(int client_fd);
void Request_parse(Request*, const char*, const size_t);
void Request_free(Request*);

#endif
