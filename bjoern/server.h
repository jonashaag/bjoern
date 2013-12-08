#ifndef __server_h__
#define __server_h__

typedef struct {
  int sockfd;
  PyObject* wsgi_app;
  PyObject* host;
  PyObject* port;
} ServerInfo;

void server_run(ServerInfo*);

#endif
