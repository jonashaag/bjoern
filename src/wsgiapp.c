#include "bjoernmodule.h"
#include "wsgiapp.h"
#include "sendfile.h"
#include "utils.h"

#define SERVER_ERROR do { DEBUG("Error on line %d", __LINE__); goto error; } while(0)

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

    if(PyFile_Check(return_value)) {
        if(wsgi_app_handle_file(request, return_value))
            goto done;
    }

    if(PyString_Check(return_value)) {
        /* We already have a string. That's ok, take it for the response. */
        if(wsgi_app_handle_string(request, return_value))
            goto done;
    }

    PyObject* iter = PyObject_GetIter(return_value);
    if(iter) {
        Py_DECREF(return_value);
        if(wsgi_app_handle_iterable(request, iter))
            goto done;
    } else { PyErr_Clear(); }

    if(!PyErr_Occurred())
        PyErr_Format(
            PyExc_TypeError,
            "start_response return value must be a "
            "file object, string or an iterable, not %.200s",
            Py_TYPE(return_value)->tp_name
        );
    goto error;

error:
    Py_XDECREF(return_value);
    Py_XDECREF(request->headers);
    Py_XDECREF(request->status);
    PyErr_Print();
    set_http_500_response(request);
    goto cleanup;

done:
    request->headers = PyObject_GetAttr(wsgi_object, _(response_headers));
    if(!validate_header_list(request->headers))
        goto error;
    if(PyErr_Occurred())
        goto error;

    request->status = PyObject_GetAttr(wsgi_object, _(response_status));
    if(request->status == NULL || !PyString_Check(request->status)) {
        PyErr_Format(
            PyExc_TypeError,
            "response_status must be a string, not %.200s",
            Py_TYPE(request->status ? request->status : Py_None)->tp_name
        );
        goto error;
    }

    goto cleanup;

cleanup:
    Py_XDECREF(args1);
    Py_XDECREF(args2);
    Py_XDECREF(wsgi_object);

    GIL_UNLOCK();
}

static bool
wsgi_app_handle_string(Request* request, PyObject* string)
{
    request->response_body = string;
    request->response_body_length = PyString_GET_SIZE(string);
    request->response_body_position = PyString_AS_STRING(string);
    request->response_type = RT_STRING;
    return true;
}

static bool
wsgi_app_handle_file(Request* request, PyObject* fileobj)
{
    request->response_type = RT_FILE;
    return wsgi_response_sendfile_init(request, (PyFileObject*)fileobj);
}

static bool
wsgi_app_handle_iterable(Request* request, PyObject* iterable)
{
    request->response_type = RT_ITERABLE;
    request->response_body = iterable;
    request->response_body_position = PyIter_Next(iterable);
    PyObject* len_method = PyObject_GetAttr(iterable, _(__len__));
    if(len_method == NULL) {
        PyErr_Clear();
        return true;
    }
    PyObject* tmp = PyObject_CallObject(len_method, NULL);
    if(tmp == NULL) {
        PyErr_Print();
        return false;
    }
    request->response_body_length = PyInt_AsLong(tmp);
    Py_DECREF(tmp);
    Py_DECREF(len_method);

    return true;
}
