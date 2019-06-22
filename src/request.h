#ifndef __request_h__
#define __request_h__

#include <ev.h>
#include "../vendors/http-parser/http_parser.h"
#include "common.h"
#include "server.h"

void _initialize_request_module();

typedef struct http_parser_url http_parser_url;

typedef struct {
    int error_code: 32;
    unsigned parse_finished : 1;
    unsigned start_response_called : 1;
    unsigned wsgi_call_done : 1;
    unsigned keep_alive : 1;
    unsigned response_length_unknown : 1;
    unsigned chunked_response : 1;
    unsigned upgrade : 1;
} request_state;

typedef struct {
    http_parser parser;
    http_parser_url url_parser;
    char *field;
    int last_call_was_header_value;
    int invalid_header;
} bj_parser;

typedef struct {
    ev_io accept_watcher;
    size_t payload_size;
    size_t header_fields;
} ThreadInfo;

typedef struct {
#ifdef DEBUG
    unsigned long id;
#endif
    int client_fd;
    int is_final;
    char *status;
    char *client_addr;

    bj_parser parser;
    ev_io ev_watcher;
    ThreadInfo *thread_info;
    request_state state;

    PyObject *headers;
    PyObject *current_chunk;
    Py_ssize_t current_chunk_p;
    PyObject *iterable;
    PyObject *iterator;

} Request;

#define REQUEST_FROM_WATCHER(watcher) \
  (Request*)((size_t)watcher - (size_t)(&(((Request*)NULL)->ev_watcher)));

Request *Request_new(ThreadInfo *, int client_fd, char *client_addr);

void Request_parse(Request *, char *, size_t);

void Request_reset(Request *);

void Request_clean(Request *);

void Request_free(Request *);

#endif
