#include "bjoern.h"
#include "request.h"

void wsgi_app(Request*);

static bool wsgi_app_handle_string(Request*, PyObject*);
static bool wsgi_app_handle_file(Request*, PyObject*);
static bool wsgi_app_handle_iterable(Request*, PyObject*);
