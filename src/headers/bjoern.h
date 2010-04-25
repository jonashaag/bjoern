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


typedef struct ev_loop* EV_LOOP;
typedef enum http_method
        http_method;
typedef struct _bjoern_http_parser
        bjoern_http_parser;

typedef enum {
    WSGI_APPLICATION_HANDLER = 1
} request_handler;

typedef enum {
    SOCKET_FAILED = -1,
    BIND_FAILED   = -2,
    LISTEN_FAILED = -3
} bjoern_network_error;

static const char* SOCKET_ERROR_MESSAGES[] = {
    "socket() failed",
    "bind() failed",
    "listen() failed"
};

static PyGILState_STATE _GILState;
static int          sockfd;
static EV_LOOP     mainloop;
static PyObject*    wsgi_application;
static PyObject*    wsgi_layer;

typedef struct _Transaction {
    int client_fd;

    ev_io       read_watcher;
    size_t      read_seek;
    /* TODO: put request_* into a seperate data structure. */
    bjoern_http_parser* request_parser;

    const char* request_url;
    http_method request_method;
    PyObject*   wsgi_environ;

    /* Write stuff: */
    ev_io       write_watcher;
    size_t      response_remaining;
    PyObject*   response_file;
    PyObject*   response_status;
    PyObject*   response_headers;
    bool        headers_sent;
    const char* response;
    request_handler request_handler;
} Transaction;

static Transaction* Transaction_new();
#define Transaction_free(t) Py_DECREF(t->wsgi_environ); \
                            Py_XDECREF(t->response_headers); \
                            Py_XDECREF(t->response_status); \
                            Py_XDECREF(t->response_file); \
                            free(t->request_parser); \
                            free(t)


#define ev_io_callback  void
#define ev_signal_callback  void

static PyObject*        Bjoern_Run          (PyObject* self, PyObject* args);
static int              init_socket         (const char* addr, const int port);
static ev_signal_callback on_sigint_received(EV_LOOP, ev_signal* watcher, const int revents);
static ev_io_callback   on_sock_accept      (EV_LOOP, ev_io* watcher, const int revents);
static ev_io_callback   on_sock_read        (EV_LOOP, ev_io* watcher, const int revents);

static ev_io_callback   while_sock_canwrite (EV_LOOP, ev_io* watcher, const int revents);
static ssize_t          bjoern_http_response(Transaction*);
static void             bjoern_send_headers (Transaction*);
static ssize_t          bjoern_sendfile     (Transaction*);
static bool             wsgi_request_handler(Transaction*);


#include "shortcuts.h"
#include "parsing.h"
#include "utils.c"
#include "strings.c"

#include "config.h"


#endif /* __bjoern_dot_h__ */
