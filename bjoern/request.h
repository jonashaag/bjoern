#ifndef __request_h__
#define __request_h__

#include <ev.h>
#include "http_parser.h"
#include "common.h"
#include "server.h"

void _initialize_request_module(ServerInfo* server_info);

typedef struct {
  unsigned error_code : 2;
  unsigned parse_finished : 1;
  unsigned start_response_called : 1;
  unsigned wsgi_call_done : 1;
  unsigned keep_alive : 1;
  unsigned response_length_unknown : 1;
  unsigned chunked_response : 1;
  unsigned expect_continue : 1;
} request_state;

typedef struct {
  http_parser parser;
  PyObject* field;
  int last_call_was_header_value;
  int invalid_header;
} bj_parser;

typedef struct {
#ifdef DEBUG
  unsigned long id;
#endif
  bj_parser parser;
  ev_io ev_watcher;

  ServerInfo* server_info;
  int client_fd;
  PyObject* client_addr;

  request_state state;

  PyObject* status;
  PyObject* headers;
  PyObject* current_chunk;
  Py_ssize_t current_chunk_p;
  PyObject* iterable;
  PyObject* iterator;
} Request;

#define REQUEST_FROM_WATCHER(watcher) \
  (Request*)((size_t)watcher - (size_t)(&(((Request*)NULL)->ev_watcher)));

Request* Request_new(ServerInfo*, int client_fd, const char* client_addr);
void Request_parse(Request*, const char*, const size_t);
void Request_reset(Request*);
void Request_clean(Request*);
void Request_free(Request*);

#endif
