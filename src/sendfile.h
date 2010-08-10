#ifndef WANT_SENDFILE
#error "no sendfile"
#endif

#include <sys/sendfile.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include "bjoern.h"
#include "request.h"
#include "response.h"
#include "mimetype.h"

bool wsgi_response_sendfile_init(Request*, PyFileObject*);
int  wsgi_response_sendfile(Request*);
