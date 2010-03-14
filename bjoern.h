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

#include <ev.h>

#include "config.h"

#define EV_LOOP struct ev_loop*
#define READ_BUFFER_SIZE 4096
#define MAX_LISTEN_QUEUE_LENGTH 1024

/* a simple boolean type */
typedef enum {
    false = 0,
    true = 1
} bool;

struct Transaction {
    int     client_fd;

    ev_io   read_watcher;
    size_t  read_seek;
    char*   request;
    char*   request_body;

    ev_io   write_watcher;
    size_t  write_seek;
    char*   response;
};

static struct Transaction* Transaction_new();
static void Transaction_free(struct Transaction*);
