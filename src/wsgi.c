#define SERVER_ERROR do { DEBUG("Error on line %d", __LINE__); goto http_500_internal_server_error; } while(0)
#ifdef WANT_SENDFILE
#  include "wsgi_sendfile.c"
#else
#  include "wsgi_mmap.c"
#endif

static bool
wsgi_call_app(Transaction* transaction)
{
    bool retval = true;
    PyObject* args1 = NULL;
    PyObject* args2 = NULL;
    PyObject* return_value = NULL;
    PyObject* wsgi_object = NULL;

#ifdef WANT_ROUTING
    Route* route = transaction->route;
#endif

    GIL_LOCK();

    /* Create a new instance of the WSGI layer class. */
    if(! (args1 = PyTuple_Pack(/* size */ 1, transaction->request_environ)))
        SERVER_ERROR;
    if(! (wsgi_object = PyObject_CallObject(wsgi_layer, args1)))
        SERVER_ERROR;

    /* Call the WSGI application: app(environ, start_response, [**kwargs]). */
    if(! (args2 = PyTuple_Pack(/* size */ 2, transaction->request_environ, wsgi_object)))
        SERVER_ERROR;

#ifdef WANT_ROUTING
    if(! (return_value = PyObject_Call(route->wsgi_callback, args2, transaction->route_kwargs)))
        SERVER_ERROR;
#else
    if(! (return_value = PyObject_CallObject(wsgi_application, args2)))
        SERVER_ERROR;
#endif

    /* Make sure to fetch the `_response_headers` attribute before anything else. */
    transaction->headers = PyObject_GetAttr(wsgi_object, PYSTRING(response_headers));
    if(!PyTuple_Check(transaction->headers)) {
        PyErr_Format(
            PyExc_TypeError,
            "response_headers must be a tuple, not %.200s",
            Py_TYPE(transaction->headers)->tp_name
        );
        Py_DECREF(transaction->headers);
        transaction->headers = PyTuple_Pack(/* size */ 0);
        goto http_500_internal_server_error;
    } else {
        Py_INCREF(transaction->headers);
    }

    if(PyFile_Check(return_value)) {
        goto file_response;
    }

    if(PyString_Check(return_value)) {
        /* We already have a string. That's ok, take it for the response. */
        transaction->body = return_value;
        transaction->body_length = PyString_Size(return_value);
        transaction->body_position = PyString_AsString(return_value);
        goto response;
    }

    if(PySequence_Check(return_value)) {
        /* WSGI-compliant case, we have a list or tuple --
           just pick the first item of that (for now) for the response.
           FIXME. */
        PyObject* item_at_0 = PySequence_GetItem(return_value, 0);
        Py_INCREF(item_at_0);
        Py_DECREF(return_value);
        transaction->body = item_at_0;
        transaction->body_length = PyString_Size(item_at_0);
        transaction->body_position = PyString_AsString(item_at_0);
        goto response;
    }

    /* Not a string, not a file, no list/tuple/sequence -- TypeError */
    SERVER_ERROR;


http_500_internal_server_error:
    transaction->status = PYSTRING(500_INTERNAL_SERVER_ERROR);
    assert(transaction->status);
    retval = false;
    goto cleanup;

file_response:
    transaction->use_sendfile = true;
    if(!wsgi_sendfile_init(transaction, (PyFileObject*)return_value))
        goto http_500_internal_server_error;
    goto response;

response:
    transaction->status = PyObject_GetAttr(wsgi_object, PYSTRING(response_status));
    if(!PyString_Check(transaction->status)) {
        PyErr_Format(
            PyExc_TypeError,
            "response_status must be a string, not %.200s",
            Py_TYPE(transaction->status)->tp_name
        );
        Py_DECREF(transaction->status);
        goto http_500_internal_server_error;
    } else {
        Py_INCREF(transaction->status);
    }
    goto cleanup;

cleanup:
    Py_XDECREF(args1);
    Py_XDECREF(args2);
    Py_XDECREF(wsgi_object);

    GIL_UNLOCK();

    return retval;
}

static response_status
wsgi_send_response(Transaction* transaction)
{
    /* TODO: Maybe it's faster to write HTTP headers and request body at once? */
    if(!transaction->headers_sent) {
        /* First time we write to this client, send HTTP headers. */
        wsgi_send_headers(transaction);
        transaction->headers_sent = true;
        return RESPONSE_NOT_YET_FINISHED;
    } else {
        if(transaction->use_sendfile)
            return wsgi_sendfile(transaction);
        else
            return wsgi_send_body(transaction);
    }
}

static response_status
wsgi_send_body(Transaction* transaction)
{
    ssize_t bytes_sent = write(
        transaction->client_fd,
        transaction->body_position,
        transaction->body_length
    );
    if(bytes_sent == -1) {
        /* socket error [note 1] */
        if(errno == EAGAIN)
            return RESPONSE_NOT_YET_FINISHED; /* Try again next time. */
        else
            return RESPONSE_SOCKET_ERROR_OCCURRED;
    }
    else {
        transaction->body_length -= bytes_sent;
        transaction->body_position += bytes_sent;
    }

    if(transaction->body_length == 0)
        return RESPONSE_FINISHED;
    else
        return RESPONSE_NOT_YET_FINISHED;
}

static void
wsgi_send_headers(Transaction* transaction)
{
    /* TODO: Maybe sprintf is faster than bjoern_strcpy? */

    char        header_buffer[MAX_HEADER_SIZE];
    char*       buffer_position = header_buffer;
    char*       orig_buffer_position = buffer_position;
    bool        have_content_length = false;
    PyObject*   header_tuple;

    GIL_LOCK();

    #define BUF_CPY(s) bjoern_strcpy(&buffer_position, s)

    /* Copy the HTTP status message into the buffer: */
    BUF_CPY("HTTP/1.1 ");
    BUF_CPY(PyString_AsString(transaction->status));
    BUF_CPY("\r\n");

    assert(transaction->headers);

    size_t header_tuple_length = PyTuple_GET_SIZE(transaction->headers);
    for(unsigned int i=0; i<header_tuple_length; ++i)
    {
        header_tuple = PyTuple_GET_ITEM(transaction->headers, i);
        BUF_CPY(PyString_AsString(PyTuple_GetItem(header_tuple, 0)));
        BUF_CPY(": ");
        BUF_CPY(PyString_AsString(PyTuple_GetItem(header_tuple, 1)));
        BUF_CPY("\r\n");
    }

    /* Make sure a Content-Length header is set: */
    if(!have_content_length)
        buffer_position += sprintf(
            buffer_position,
            "Content-Length: %zu\r\n",
            transaction->body_length
        );

    BUF_CPY("\r\n");

    #undef BUF_CPY

    write(transaction->client_fd, header_buffer,
          buffer_position - orig_buffer_position);

    Py_DECREF(transaction->headers);
    Py_DECREF(transaction->status);

    GIL_UNLOCK();
}

static inline void
wsgi_finalize(Transaction* transaction)
{
    GIL_LOCK();
    DEBUG("refc: %d", transaction->body->ob_refcnt);
    DEBUG("refc: %d", transaction->request_environ->ob_refcnt);
    DEBUG("refc: %d", transaction->body->ob_refcnt);
    Py_DECREF(transaction->route_kwargs);
    Py_XDECREF(transaction->request_environ);
#ifdef WANT_ROUTING
    Py_XDECREF(transaction->route_kwargs);
#endif
    GIL_UNLOCK();
}
