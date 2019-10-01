#ifndef __server_h__
#define __server_h__

#include "statsd-client.h"

typedef struct {
  int sockfd;
  PyObject* wsgi_app;
  PyObject* host;
  PyObject* port;
} ServerInfo;

typedef struct {
  int enabled;
  int port;
  char* host;
  char* namespace;
} StatsdInfo;

void server_run(ServerInfo*, statsd_link*);

#endif
