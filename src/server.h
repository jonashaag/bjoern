#ifndef __server_h__
#define __server_h__

#include <stdio.h>
#include <ev.h>

typedef struct {
    int sockfd;
    PyObject *max_body_len;
    PyObject *max_header_fields;
    PyObject *max_header_field_len;
    PyObject *wsgi_app;
    PyObject *host;
    PyObject *port;
    PyObject *log_console_level;
    PyObject *log_file_level;
    PyObject *log_file;
} ServerInfo;

typedef struct {
    ServerInfo *server_info;
    ev_io accept_watcher;
    size_t payload_size;
    size_t header_fields;
} ThreadInfo;

void server_run(ServerInfo *);

#endif
