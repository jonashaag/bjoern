#ifndef __bjoern_dot_h__
#define __bjoern_dot_h__

#include <Python.h>
#include <ev.h>
#include <http_parser.h>

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stddef.h>
#include <sys/types.h>

#include <sys/socket.h>
#include <sys/sendfile.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>

#include "config.h"
#include "shortcuts.h"
#include "utils.c"

#define READ_BUFFER_SIZE        4096
#define WRITE_SIZE              50*4096
#define MAX_HEADER_SIZE         1<<13 /* Average value looking at other server's code */
#define MAX_LISTEN_QUEUE_LENGTH 1024
#define DEFAULT_RESPONSE_CONTENT_TYPE "text/plain"

#define SOCKET_FAILED -1
#define BIND_FAILED   -2
#define LISTEN_FAILED -3

static const char* HTTP_500_MESSAGE = "HTTP 500 Internal Server Error. Try again later.";

static char* socket_error_format(const int errnum) {
    switch(errnum) {
        case SOCKET_FAILED:
            return "socket() failed";
        case BIND_FAILED:
            return "bind() failed";
        case LISTEN_FAILED:
            return "listen() failed";
        default:
            /* Mustn't happen */
            assert(0);
    }
}


static PyGILState_STATE _GILState;
static int          sockfd;
static EV_LOOP      mainloop;
static PyObject*    wsgi_application;
static PyObject*    wsgi_layer;

struct bj_http_parser {
    PARSER        http_parser;
    TRANSACTION*  transaction;

    /* Temporary variables: */
    const char*   header_name_start;
    size_t        header_name_length;
    const char*   header_value_start;
    size_t        header_value_length;
};

enum http_parser_error {
    HTTP_PARSER_ERROR_REQUEST_METHOD_NOT_SUPPORTED = 1
};


typedef enum http_method http_method;

TRANSACTION {
    IF_DEBUG(int num;)

    int client_fd;

    ev_io       read_watcher;
    size_t      read_seek;
    /* TODO: put request_* into a seperate data structure. */
    BJPARSER*   request_parser;

    const char* request_url;
    http_method request_method;
    PyObject*   wsgi_environ;


    /* Write stuff: */
    ev_io       write_watcher;
    size_t      response_remaining;
    PyObject*   response_file;
    PyObject*   response_headers;
    bool        headers_sent;
    const char* response;
};

static TRANSACTION* Transaction_new();
#define Transaction_free(t) Py_DECREF(t->wsgi_environ); \
                            Py_XDECREF(t->response_headers); \
                            Py_XDECREF(t->response_file); \
                            free(t->request_parser); \
                            free(t)

#endif /* __bjoern_dot_h__ */
