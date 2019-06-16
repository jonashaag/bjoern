#ifndef __server_h__
#define __server_h__

#include <stdio.h>
#include <ev.h>

typedef struct {
    int sockfd;
    size_t max_body_len;
    size_t max_header_fields;
    size_t max_header_field_len;
    PyObject *wsgi_app;
    const char *host;
    int port;
} ServerInfo;

ServerInfo *get_server_info();
void set_server_info(ServerInfo *info);
void server_run();

#define SERVER_INFO get_server_info()

#endif
