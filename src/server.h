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
} ServerInfo;

ServerInfo *get_server_info();
void set_server_info(ServerInfo *info);
void server_run();

#define SERVER_INFO get_server_info()

#endif
