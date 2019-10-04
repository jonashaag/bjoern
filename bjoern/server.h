#ifndef __server_h__
#define __server_h__

#ifdef WANT_STATSD
#include "statsd-client.h"
#endif

typedef struct {
  int sockfd;
  PyObject* wsgi_app;
  PyObject* host;
  PyObject* port;
#ifdef WANT_STATSD
  statsd_link* statsd;
# ifdef WANT_STATSD_TAGS
  char* statsd_tags;
# endif
#endif
} ServerInfo;

void server_run(ServerInfo*);

#endif
