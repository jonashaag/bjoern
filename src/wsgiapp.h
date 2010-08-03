#include "bjoern.h"
#include "request.h"

void wsgi_app(Request*);

static void wsgi_app_handle_string(Request*, PyObject*);
static void wsgi_app_handle_file(Request*, PyObject*);
static void wsgi_app_handle_iterable(Request*, PyObject*);
