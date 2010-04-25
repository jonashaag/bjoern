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


typedef struct ev_loop
        EV_LOOP;
typedef struct _Transaction
        Transaction;
typedef struct _bjoern_http_parser
        bjoern_http_parser;
typedef enum http_method
        http_method;

typedef enum {
    REQUEST_METHOD_NOT_SUPPORTED = 1
} http_parser_error;

typedef enum {
    WSGI_APPLICATION_HANDLER = 1
} request_handler;


#include "shortcuts.h"
#include "transaction.h"
#include "parsing.h"
#include "utils.c"
#include "strings.c"

#include "config.h"


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
static EV_LOOP*     mainloop;
static PyObject*    wsgi_application;
static PyObject*    wsgi_layer;


#define ev_io_callback  void
#define ev_signal_callback  void

static PyObject*        Bjoern_Run          (PyObject* self, PyObject* args);
static int              init_socket         (const char* addr, const int port);
static ev_signal_callback on_sigint_received(EV_LOOP*, ev_signal* watcher, const int revents);
static ev_io_callback   on_sock_accept      (EV_LOOP*, ev_io* watcher, const int revents);
static ev_io_callback   on_sock_read        (EV_LOOP*, ev_io* watcher, const int revents);

static ev_io_callback   while_sock_canwrite (EV_LOOP*, ev_io* watcher, const int revents);
static ssize_t          bjoern_http_response(Transaction*);
static void             bjoern_send_headers (Transaction*);
static ssize_t          bjoern_sendfile     (Transaction*);
static bool             wsgi_request_handler(Transaction*);

#endif /* __bjoern_dot_h__ */
