#ifndef __request_h__
#define __request_h__

#include <ev.h>
#include "common.h"

typedef struct {
    int client_fd;
    ev_io ev_watcher;
    PyObject* headers;
} Request;

Request* Request_new(int client_fd);
bool Request_parse(Request*, const char*, const size_t);
void Request_write_response(Request*, const char*, size_t);
void Request_free(Request*);

#endif
