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


typedef struct ev_loop EV_LOOP;
typedef struct _Transaction Transaction;
typedef struct _bjoern_http_parser bjoern_http_parser;
typedef struct _wsgi_handler_data wsgi_handler_data;
typedef struct _raw_handler_data raw_handler_data;
#ifdef WANT_CACHING
typedef struct _cache_handler_data cache_handler_data;
#endif

typedef enum { RESPONSE_FINISHED = 1, RESPONSE_NOT_YET_FINISHED = 2} response_status;

#include "shortcuts.h"
#include "http-status-codes.h"
#include "strings.h"
#include "utils.c"
#include "parsing.h"

#ifdef WANT_ROUTING
#include "routing.h"
#endif

#include "handlers/wsgi.h"
#include "handlers/raw.h"

#ifdef WANT_CACHING
#include "handlers/cache.h"
#endif

#include "transaction.h"

#include "config.h"


static int          sockfd;
static EV_LOOP*     mainloop;
static PyObject*    wsgi_layer;
#ifndef WANT_ROUTING
  /* No routing, use one application for every request */
static PyObject*    wsgi_application;
#endif
static PyGILState_STATE _GILState;


#define ev_io_callback void
#define ev_signal_callback void

static PyObject*        Bjoern_Run          (PyObject* self, PyObject* args);
static ssize_t          init_socket         (const char* addr, const int port);
static ev_signal_callback on_sigint_received(EV_LOOP*, ev_signal* watcher, const int revents);
static ev_io_callback   on_sock_accept      (EV_LOOP*, ev_io* watcher, const int revents);
static ev_io_callback   on_sock_read        (EV_LOOP*, ev_io* watcher, const int revents);
static ev_io_callback   while_sock_canwrite (EV_LOOP*, ev_io* watcher, const int revents);
