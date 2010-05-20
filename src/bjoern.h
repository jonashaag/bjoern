#include <Python.h>
#include <ev.h>
#include <http_parser.h>
#include <pthread.h>

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
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


#include "utils.h"
#include "debug.h"
#include "stringcache.h"
#include "http_status_codes.h"

#ifdef WANT_ROUTING
  #include "routing.h"
#endif
#include "parsing.h"
#include "handlers/wsgi.h"
#include "handlers/raw.h"
#ifdef WANT_CACHING
  #include "handlers/cache.h"
#endif

#include "transaction.h"
#include "config.h"


static int sockfd;
static bool shall_cleanup;
static PyObject* wsgi_layer;
#ifndef WANT_ROUTING
  /* No routing, use one application for every request */
static PyObject*    wsgi_application;
#endif


typedef void ev_io_callback(EV_LOOP*, ev_io* watcher, const int revents);
typedef void ev_signal_callback(EV_LOOP*, ev_signal* watcher, const int revents);

static PyObject* Bjoern_Run(PyObject* self, PyObject* args);
static ssize_t init_socket(const char* addr, const int port);
static void bjoern_cleanup(EV_LOOP*);
static ev_io_callback on_sock_accept;
static ev_io_callback on_sock_read;
static ev_io_callback while_sock_canwrite;
static ev_signal_callback on_sigint;
