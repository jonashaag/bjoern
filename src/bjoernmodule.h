#ifndef __bjoernmodule_h__
#define __bjoernmodule_h__

#include "bjoern.h"
#include "routing.h"
#include "request.h"

/* Public */
bool bjoern_flush_errors();
void bjoern_server_error();

PyObject* response_class;
#ifndef WANT_ROUTING
/* No routing, use one application for every request */
PyObject* wsgi_application;
#endif


/* Internal */
static int sockfd;

static PyObject* Bjoern_Run(PyObject* self, PyObject* args);
static PyObject* Bjoern_Route_Add(PyObject* self, PyObject* args);
static ssize_t init_socket(c_char* addr, const int port);

static ev_signal_callback on_sigint;

static bool shall_cleanup;
static void bjoern_cleanup();

#endif
