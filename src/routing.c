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
    _re_compile = PyObject_GetAttrString(re_module, "compile");
}

static PyRegex*
re_compile(PyObject* pattern)
{
    PyObject* args = PyTuple_Pack(/* size */ 1, pattern);
    PyRegex* regex = PyObject_CallObject(_re_compile, args);
    Py_DECREF(args);
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

    Route* new_route = Route_new(pattern, callback);

    if(first_route == NULL)
        /* The very first route defined. */
        first_route = new_route;
    else
        last_route->next = new_route;
    last_route = new_route;

    Py_RETURN_NONE;
}


static Route*
Route_new(PyObject* pattern, PyObject* wsgi_callback)
{
    Route* route = malloc(sizeof(Route));
    if(route == NULL)
        return NULL;

    route->next = NULL;
    route->wsgi_callback = wsgi_callback;

    route->pattern = re_compile(pattern);
    if(route->pattern == NULL) {
        Route_free(route);
        return NULL;
    }
    route->pattern_match_func = PyObject_GetAttrString(route->pattern, "match");
    if(route->pattern_match_func == NULL) {
        Route_free(route);
        return NULL;
    }

    Py_INCREF(route->pattern_match_func);
    return route;
}


static void
get_route_for_url(PyObject* url, Route** route_, PyObject** matchdict_)
{
    PyObject* args = PyTuple_Pack(1, url);
    PyObject* matchobj;

    GIL_LOCK();

    Route* route = first_route;
    while(route) {
        matchobj = PyObject_CallObject(route->pattern_match_func, args);
        if(matchobj != Py_None) {
            /* Match successful */
            PyObject* groupdict_method = PyObject_GetAttr(matchobj, PYSTRING(groupdict));
            PyObject* matchdict = PyObject_CallObject(groupdict_method, NULL);
            Py_DECREF(groupdict_method);
            Py_DECREF(matchobj);
            *route_ = route;
            *matchdict_ = matchdict;
            goto cleanup;
        }
        Py_DECREF(matchobj);
        route = route->next;
    }

cleanup:
    Py_DECREF(args);
    GIL_UNLOCK();
}
