#ifndef __request_h__
#define __request_h__

#include <ev.h>
#include "http_parser.h"
#include "common.h"

typedef enum {
    REQUEST_FRESH = 1,
    REQUEST_READING,
    REQUEST_PARSE_ERROR,
    REQUEST_PARSE_DONE,
    REQUEST_WSGI_GENERAL_RESPONSE,
    REQUEST_WSGI_STRING_RESPONSE,
    REQUEST_WSGI_FILE_RESPONSE,
    REQUEST_WSGI_ITER_RESPONSE
} request_state;

typedef struct {
    http_parser parser;
    const char* field_start;
    size_t field_len;
    const char* value_start;
    size_t value_len;
} bj_parser;

typedef struct {
    request_state state;
    int client_fd;
    ev_io ev_watcher;

    bj_parser parser;
    PyObject* headers;

    void* response;
    PyObject* response_headers;
    PyObject* status;
    PyObject* response_curiter;
} Request;

Request* Request_new(int client_fd);
void Request_parse(Request*, const char*, const size_t);
void Request_write_response(Request*, const char*, size_t);
void Request_free(Request*);

#endif
