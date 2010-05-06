typedef PyObject
        PyRegex;

struct _Route {
    Route* next;
    PyRegex* pattern;
    PyObject* pattern_match_func; /* pattern.match */
    PyObject* wsgi_callback;
};

static Route* Route_new(PyObject* pattern, PyObject* wsgi_callback);
#define Route_free(route) do { \
                            Py_XDECREF(route->pattern); \
                            Py_XDECREF(route->pattern_match_func); \
                            Py_XDECREF(route->wsgi_callback); \
                            free(route); \
                          } while(0)

Route* first_route;
Route* last_route;
static PyObject* _re_compile;

static PyObject* Bjoern_Route_Add(PyObject* self, PyObject* args);
static Route* get_route_for_url(PyObject* url);
static void init_routing();
static void import_re_module();
