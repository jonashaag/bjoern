#ifndef __routing_h__
#define __routing_h__

#include "Python.h"

typedef struct _Route {
    struct _Route* next;
    PyObject* pattern;
    PyObject* pattern_match_func; /* pattern.match */
    PyObject* wsgi_callback;
} Route;

Route* Route_new(PyObject* pattern, PyObject* wsgi_callback);
void Route_add(Route*);
void Route_free(Route*);
void get_route_for_url(PyObject* url, Route** route, PyObject** matchdict);
void routing_init();

#endif
