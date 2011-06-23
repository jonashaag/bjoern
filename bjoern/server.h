#include "request.h"

bool server_init(const char* hostaddr, const int port);
void server_run();

#ifdef TLS_SUPPORT
bool create_tls_context(char* key, char* cert, char* cipher);
#endif