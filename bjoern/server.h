
#ifndef BJOERN_SERVER_H_
#define BJOERN_SERVER_H_ 1

#include "request.h"

bool server_init(const char* hostaddr, const int port);
void server_run();

#endif /* !BJOERN_SERVER_H_ */
