static inline void
init_routing()
{
    first_route = NULL;
    last_route  = NULL;
}

static void
import_re_module()
{
    PyObject* re_module = PyImport_ImportModule("re");
    _re_compile = PyGetAttr(re_module, "compile");
    Py_INCREF(_re_compile);
}

static PyRegex*
re_compile(PyObject* pattern)
{
    PyObject* args = PyTuple_Pack(/* size */ 1, pattern);
    Py_INCREF(args);

    PyRegex* regex = PyObject_Call(_re_compile, args, /* kwargs */ NULL);
    if(regex == NULL)
        return NULL;
    Py_INCREF(regex);
    return regex;
}

PyObject* Bjoern_Route_Add(PyObject* self, PyObject* args)
{
    PyObject* pattern;
    PyObject* callback;

    if(!PyArg_ParseTuple(args, "SO", &pattern, &callback))
        return NULL;

    Py_INCREF(pattern);
    Py_INCREF(callback);

    Route* new_route = Route_new(pattern, callback, last_route);
    if(first_route == NULL)
        first_route = new_route;
    last_route = new_route;

    Py_RETURN_NONE;
}


static Route*
Route_new(PyObject* pattern, PyObject* wsgi_callback, Route* next)
{
    Route* route = malloc(sizeof(Route));

    route->next = next;
    route->wsgi_callback = wsgi_callback;

    route->pattern = re_compile(pattern);
    if(route->pattern == NULL) {
        Route_free(route);
        return NULL;
    }
    route->pattern_match_func = PyGetAttr(route->pattern, "match");
    if(route->pattern_match_func == NULL) {
        Route_free(route);
        return NULL;
    }

    return route;
}


static Route*
get_route_for_url(PyObject* url)
{
    PyObject* args = Py_BuildValue("(s)", url);
    Py_INCREF(args);
    Route* route = first_route;

    PyObject* py_tmp;
    while(route) {
        py_tmp = PyObject_Call(route->pattern_match_func, args, /* kwargs */ NULL);
        if(py_tmp && py_tmp != Py_None) {
            /* Match successful */
            goto cleanup;
        }
        route = route->next;
    }

cleanup:
    Py_DECREF(args);
    return route;
}
