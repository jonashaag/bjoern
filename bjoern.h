#ifndef __bjoern_dot_h__
#define __bjoern_dot_h__

#include <Python.h>
#include <ev.h>
#include <http_parser.h>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stddef.h>

#include <fcntl.h>
#include <errno.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>

#include "config.h"


#ifdef DEBUG_ON
  #define DEBUG(s, ...)     fprintf(stdout, s"\n", ## __VA_ARGS__ ); fflush(stdout)
#else
  #define DEBUG(...)
#endif
#define ERROR(s, ...)       fprintf(stderr, s"\n", ## __VA_ARGS__ ); fflush(stderr)

#define EV_LOOP struct ev_loop*
#define READ_BUFFER_SIZE 4096
#define MAX_LISTEN_QUEUE_LENGTH 1024

#define EXIT_CODE_SOCKET_FAILED -1
#define EXIT_CODE_BIND_FAILED   -2
#define EXIT_CODE_LISTEN_FAILED -3


static int sockfd;

/* a simple boolean type */
typedef enum {
    false = 0,
    true = 1
} bool;


#define TRANSACTION struct Transaction
#define PARSER   struct http_parser
#define BJPARSER struct bj_http_parser

struct bj_http_parser {
    PARSER*       httpparser;
    TRANSACTION*  transaction;

    /* Temporary variables: */
    const char*   header_name_start;
    size_t        header_name_length;
    const char*   header_value_start;
    size_t        header_value_length;
};


#define TRANSACTION_FROM_PARSER(parser) ((TRANSACTION*)parser->data)
TRANSACTION {
    int client_fd;
    int num;

    ev_io       read_watcher;
    size_t      read_seek;
    /* Todo: put request_* into a seperate data structure. */
    BJPARSER*   request_parser;

    PyObject*   request_status_code;
    PyObject*   request_url;
    PyObject*   request_query;
    PyObject*   request_url_fragment;
    PyObject*   request_path;
    PyObject*   request_headers;
    PyObject*   request_body;


    /* Write stuff: */
    ev_io       write_watcher;
    size_t      write_seek;
    char*       response;
};

static TRANSACTION* Transaction_new();
static void Transaction_free(TRANSACTION*);

#endif /* __bjoern_dot_h__ */
