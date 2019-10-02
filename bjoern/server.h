#ifndef __server_h__
#define __server_h__

typedef struct {
  int sockfd;
  PyObject* wsgi_app;
  PyObject* host;
  PyObject* port;
} ServerInfo;

#ifdef WANT_STATSD
#include "statsd-client.h"

typedef struct {
  int enabled;
  int port;
  char* host;
  char* namespace;
} StatsdInfo;

  #ifdef WANT_STATSD_TAGS
  void server_run(ServerInfo*, statsd_link*, const char*);
  #else
  void server_run(ServerInfo*, statsd_link*);
  #endif

#else
void server_run(ServerInfo*);
#endif

#endif
