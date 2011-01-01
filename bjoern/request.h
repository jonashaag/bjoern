#ifndef __request_h__
#define __request_h__

#include <ev.h>
#include "http_parser.h"
#include "common.h"

void _request_module_initialize(const char* host, const int port);

typedef struct {
    unsigned error_code : 2;
    unsigned parse_finished : 1;
    unsigned start_response_called : 1;
    unsigned wsgi_call_done : 1;
    unsigned keep_alive : 1;
    unsigned response_length_unknown : 1;
    unsigned chunked_response : 1;
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
    bj_parser parser;
    ev_io ev_watcher;

    int client_fd;
    PyObject* client_addr;

    request_state state;

    PyObject* headers;
    PyObject* body;
    PyObject* current_chunk;
    Py_ssize_t current_chunk_p;
    PyObject* iterable;
    PyObject* status;
} Request;

Request* Request_new(int client_fd, const char* client_addr);
void Request_reset(Request*, bool decref_members);
void Request_parse(Request*, const char*, const size_t);
void Request_free(Request*);

static PyObject
    * _REMOTE_ADDR,
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
