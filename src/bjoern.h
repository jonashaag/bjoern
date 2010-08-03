#ifndef __bjoern_h__
#define __bjoern_h__

#include <Python.h>
#include <ev.h>

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

#define OFFSETOF(mbr_name, ptr, type)  ((type*) (((char*)ptr) - offsetof(type, mbr_name)))
#define GIL_LOCK() PyGILState_STATE GILState = PyGILState_Ensure()
#define GIL_UNLOCK() PyGILState_Release(GILState)

/* Python 2.5 compatibility */
#if PY_VERSION_HEX < 0x2060000
#  define Py_TYPE(o) ((o)->ob_type)
#  define PyFile_IncUseCount(file) do{}while(0)
#  define PyFile_DecUseCount(file) do{}while(0)
#endif

typedef struct ev_loop EV_LOOP;

typedef void ev_io_callback(EV_LOOP*, ev_io* watcher, const int revents);
typedef void ev_signal_callback(EV_LOOP*, ev_signal* watcher, const int revents);

typedef enum {
    HTTP_NOT_FOUND              = 404,
    HTTP_INTERNAL_SERVER_ERROR  = 500,
    HTTP_NOT_IMPLEMENTED        = 501
} http_status_code;

#define c_char const char
#define c_size_t const size_t


#include "debug.h"
#include "staticstrings.h"
#include "config.h"

#endif
