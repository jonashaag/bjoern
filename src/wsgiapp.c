#include "bjoern.h"
#include "request.h"
#include "wsgiapp.h"
#include "response.h"
#include "sendfile.h"
#include "utils.h"

#define SERVER_ERROR do { DEBUG("Error on line %d", __LINE__); goto done; } while(0)

void
wsgi_app(Request* request)
{
    PyObject* args1 = NULL;
    PyObject* args2 = NULL;
    PyObject* return_value = NULL;
    PyObject* wsgi_object = NULL;

#ifdef WANT_ROUTING
    Route* route = request->route;
#endif

    GIL_LOCK();
    DEBUG("Calling WSGI application.");

    /* Create a new instance of the WSGI layer class. */
    if(! (args1 = PyTuple_Pack(/* size */ 1, request->request_environ)))
        SERVER_ERROR;
    if(! (wsgi_object = PyObject_CallObject(response_class, args1)))
        SERVER_ERROR;

    /* Call the WSGI application: app(environ, start_response, [**kwargs]). */
    if(! (args2 = PyTuple_Pack(/* size */ 2, request->request_environ, wsgi_object)))
        SERVER_ERROR;

#ifdef WANT_ROUTING
    if(! (return_value = PyObject_Call(route->wsgi_callback, args2, request->route_kwargs)))
        SERVER_ERROR;
#else
    if(! (return_value = PyObject_CallObject(wsgi_application, args2)))
        SERVER_ERROR;
#endif

    /* Make sure to fetch the `response_headers` attribute before anything else. */
    request->headers = PyObject_GetAttr(wsgi_object, PYSTRING(response_headers));
    if(!validate_header_tuple(request->headers)) {
        Py_DECREF(request->headers);
        Py_DECREF(return_value);
        goto done;
    }

    if(PyFile_Check(return_value)) {
        wsgi_app_handle_file(request, return_value);
        goto done;
    }

    if(PyString_Check(return_value)) {
        /* We already have a string. That's ok, take it for the response. */
        wsgi_app_handle_string(request, return_value);
        goto done;
    }

    if(PySequence_Check(return_value)) {
        wsgi_app_handle_iterable(request, return_value);
        goto done;
    }
    /* TODO: iterators/generator */

    PyErr_Format(
        PyExc_TypeError,
        "start_response return value must be a "
        "file object, string or an iterable, not %.200s",
        Py_TYPE(return_value)->tp_name
    );
    goto done;

done:
    if(bjoern_flush_errors()) {
        set_http_500_response(request);
        goto cleanup;
    }

    request->status = PyObject_GetAttr(wsgi_object, PYSTRING(response_status));
    if(request->status == NULL || !PyString_Check(request->status)) {
        PyErr_Format(
            PyExc_TypeError,
            "response_status must be a string, not %.200s",
            Py_TYPE(request->status ? request->status : Py_None)->tp_name
        );
        Py_DECREF(request->status);
        bjoern_server_error();
        goto cleanup;
    }

cleanup:
    Py_XDECREF(args1);
    Py_XDECREF(args2);
    Py_XDECREF(wsgi_object);

    GIL_UNLOCK();
}

static void
wsgi_app_handle_string(Request* request, PyObject* string)
{
    request->response_body = string;
    request->response_body_length = PyString_GET_SIZE(string);
    request->response_body_position = PyString_AS_STRING(string);
}

static void
wsgi_app_handle_file(Request* request, PyObject* fileobj)
{
    request->use_sendfile = true;
    wsgi_sendfile_init(request, (PyFileObject*)fileobj);
}

static void
wsgi_app_handle_iterable(Request* request, PyObject* iterable)
{
    PyObject* item_at_0 = PySequence_GetItem(iterable, 0);
    Py_INCREF(item_at_0);
    Py_DECREF(iterable);
    request->response_body = item_at_0;
    request->response_body_length = PyString_Size(item_at_0);
    request->response_body_position = PyString_AsString(item_at_0);
}
