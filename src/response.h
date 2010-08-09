#ifndef __response_h__
#define __response_h__

#include "request.h"
#include "wsgiapp.h"
#include "sendfile.h"
#include "utils.h"

/* Internal */
static int wsgi_send_response(Request*);
static void wsgi_send_headers(Request*);
static int wsgi_send_body(Request*);
static void wsgi_finalize(Request*);


/* Public */
#define RESPONSE_FINISHED  1
#define RESPONSE_NOT_YET_FINISHED  2
#define RESPONSE_SOCKET_ERROR_OCCURRED 3

void get_response(Request*, int parser_exit_code);
void set_http_500_response(Request*);
void set_http_404_response(Request*);
void set_http_400_response(Request*);

ev_io_callback while_sock_canwrite;

#endif
