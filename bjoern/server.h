#include <ev.h>
#include "request.h"

bool server_run(const char* hostaddr, const int port);
void sendall(Request*, const char*, size_t);
