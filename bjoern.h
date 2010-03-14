#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <sys/types.h>

#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>

#include <ev.h>

#include "config.h"

#define EV_LOOP struct ev_loop*

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

    ev_io   write_watcher;
    size_t  write_seek;
    char*   response;
};

struct Transaction* Transaction_new();
void Transaction_free(struct Transaction*);
