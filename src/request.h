#ifndef __request_h__
#define __request_h__

#include <Python.h>
#include "parser.h"
#include "bjoern.h"

#ifdef WANT_ROUTING
#include "routing.h"
#endif

ev_io_callback on_sock_read;
ev_io_callback on_sock_accept;

typedef struct _Request {
    int client_fd;
    ev_io read_watcher;
    bool received_anything;
    struct _Parser* parser;
    enum http_method request_method;

    ev_io write_watcher;

    PyObject* request_environ;
#ifdef WANT_ROUTING
    Route* route;
    PyObject* route_kwargs;
#endif
    PyObject* status;
    PyObject* headers;
    bool headers_sent;
    bool use_sendfile;
    PyObject* response_body;
    size_t response_body_length;
    c_char* response_body_position;
} Request;

Request* Request_new();
void Request_free(Request*);

#include "response.h"

#endif
