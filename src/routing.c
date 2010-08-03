#include "routing.h"

Route* first_route = NULL;
Route* last_route  = NULL;
PyObject* re_module;
PyObject* _re_compile;

static PyObject* re_compile(PyObject*);

Route*
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

void
Route_free(Route* route)
{
    Py_XDECREF(route->pattern);
    Py_XDECREF(route->pattern_match_func);
    Py_XDECREF(route->wsgi_callback);
    free(route);
}

void
Route_add(Route* route)
{
    if(first_route == NULL)
        /* The very first route defined. */
        first_route = route;
    else
        last_route->next = route;
    last_route = route;
}

void
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
            PyObject* groupdict_method = PyObject_GetAttr(matchobj, _(groupdict));
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

void
routing_init()
{
    PyObject* re_module = PyImport_ImportModule("re");
    _re_compile = PyObject_GetAttrString(re_module, "compile");
}

static PyObject*
re_compile(PyObject* pattern)
{
    PyObject* args = PyTuple_Pack(/* size */ 1, pattern);
    PyObject* regex = PyObject_CallObject(_re_compile, args);
    Py_DECREF(args);
    return regex;
}

