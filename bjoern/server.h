#include <ev.h>
#include "request.h"

bool server_run(const char* hostaddr, const int port);
void send_error(struct ev_loop*, Request*, http_status);
