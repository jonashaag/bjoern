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
    PyObject* args = Py_BuildValue("(s)", pattern);
    Py_INCREF(args);

    PyRegex* regex = PyObject_Call(_re_compile, args, /* kwargs */ NULL);
    if(regex == NULL)
        return NULL;
    Py_INCREF(regex);
    return regex;
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
