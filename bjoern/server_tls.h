#include "request.h"

bool server_tls_init(const char* hostaddr, const int port, char* key, char* cert);
void server_tls_run();