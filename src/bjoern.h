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

typedef struct ev_loop EV_LOOP;

typedef void ev_io_callback(EV_LOOP*, ev_io* watcher, const int revents);
typedef void ev_signal_callback(EV_LOOP*, ev_signal* watcher, const int revents);

/* Python 2.5 compatibility */
#if PY_VERSION_HEX < 0x2060000
#  define Py_TYPE(o) ((o)->ob_type)
#  define PyFile_IncUseCount(file) do{}while(0)
#  define PyFile_DecUseCount(file) do{}while(0)
#endif

#include "staticstrings.h"
#include "debug.h"
#include "bjoernmodule.h"
#include "config.h"

#endif
