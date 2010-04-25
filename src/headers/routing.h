typedef PyObject
        PyRegex;

struct _Route {
    Route* next;
    PyRegex* pattern;
    PyObject* pattern_match_func; /* pattern.match(...) */
    PyObject* wsgi_callback;
};

static Route* Route_new(PyObject* pattern, PyObject* wsgi_callback, Route* next);
#define Route_free(route) Py_XDECREF(route->pattern); \
                          Py_XDECREF(route->pattern_match_func); \
                          Py_XDECREF(route->wsgi_callback); \
                          free(route)

Route* first_route;
static PyObject* _re_compile;

static Route* get_route_for_url(PyObject* url);
static void import_re_module();
