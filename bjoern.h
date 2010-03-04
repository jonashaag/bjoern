#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <stddef.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <fcntl.h>
#include <ev.h>


/* version information */
#define BJOERN_NAME "bjoern"
#define BJOERN_VERSION "0.1"
#define BJOERN_DESCRIPTION "the ultra lightweight and screamingly fast Python WSGI server written in C"
#define SERVER_IDENTITY BJOERN_NAME BJOERN_VERSION " -- " BJOERN_DESCRIPTION
#define DUMMY_RESPONSE ("HTTP/1.1 200 Alles Ok\r\nServer: " SERVER_IDENTITY "\r\n" \
                        "Connection: close\r\nContent-Length: 28" \
                        "\r\n\r\nHello World from bjoern " BJOERN_VERSION "!\r\n")


/* default and user configuration */
#include "config.h"

#ifndef HTTP_REQUEST_BUFFER_LENGTH
#define HTTP_REQUEST_BUFFER_LENGTH  1024
#endif
#ifndef SOCKET_MAX_CONNECTIONS
#define SOCKET_MAX_CONNECTIONS 1024
#endif


/* a simple boolean type */
typedef enum {
    false = 0,
    true = 1
} bool;

struct Client {
    ev_io ev_write;
    ev_io ev_read;
    int fd;

    char* header;
    char* body;
    size_t input_position;
};
