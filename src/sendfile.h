#ifndef WANT_SENDFILE
#error "no sendfile"
#endif

#include <sys/sendfile.h>
#include "bjoern.h"
#include "request.h"
#include "response.h"
#include "mimetype.h"

bool wsgi_sendfile_init(Request*, PyFileObject*);
int wsgi_sendfile(Request*);
