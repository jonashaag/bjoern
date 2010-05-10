static bool
wsgi_handler_initialize(Transaction* transaction)
{
    wsgi_handler_data response_data = transaction->request_handler;
    PyObject* args;
    PyObject* wsgi_object = NULL;

#ifdef WANT_ROUTING
    Route* route = response_data.route;
#endif

    /* Create a new response header dictionary. */
    response_data.wsgi_environ = PyDict_New();
    if(response_data.wsgi_environ == NULL)
        goto http_500_internal_server_error;
    Py_INCREF(response_data.wsgi_environ);

    /* Create a new instance of the WSGI layer class. */
    args = PyTuple_Pack(/* size */ 1, response_data.wsgi_environ);
    if(args == NULL)
        goto http_500_internal_server_error;
    Py_INCREF(args);
    wsgi_object = PyObject_Call(wsgi_layer, args, NULL /* kwargs */);
    Py_DECREF(args);

    if(wsgi_object == NULL)
        goto http_500_internal_server_error;
    Py_INCREF(wsgi_object);

    /* Call the WSGI application: app(environ, start_response). */
    args = PyTuple_Pack(
        /* size */ 2,
        response_data.wsgi_environ,
        PyObject_GetAttr(wsgi_object, PY_STRING_start_response)
    );
    if(args == NULL)
        goto http_500_internal_server_error;
    Py_INCREF(args);
#ifdef WANT_ROUTING
    PyObject* return_value = PyObject_Call(route->wsgi_callback,
                                           args, NULL /* kwargs */);
#else
    PyObject* return_value = PyObject_Call(wsgi_application,
                                           args, NULL /* kwargs */);
#endif
    Py_DECREF(args);


    if(return_value == NULL)
        goto http_500_internal_server_error;

    Py_INCREF(return_value);

    if(PyIter_Check(return_value)) {
        Py_DECREF(return_value); /* don't need this anymore */
        goto http_500_internal_server_error;
    }

    if(PyFile_Check(return_value)) {
        transaction->response_file = return_value;
        /* TODO: Use proper file size for `sendfile`. */
        goto file_response;
    }


    if(PyString_Check(return_value)) {
        /* We already have a string. That's ok, take it for the response. */
        response_data.body = PyString_AsString(return_value);
        goto response;
    }

    if(PySequence_Check(return_value)) {
        /* WSGI-compliant case, we have a list or tuple --
           just pick the first item of that (for now) for the response.
           FIXME. */
        response_data.body = PyString_AsString(PySequence_GetItem(return_value, 0));
        Py_DECREF(return_value); /* don't need this anymore */
        goto response;
    }

    /* Not a string, not a file, no list/tuple/sequence -- TypeError */
    goto http_500_internal_server_error;


http_500_internal_server_error:
    /* HACK: We redirect this request directly to the HTTP 500 handler. */
    transaction->request_parser->exit_code = HTTP_INTERNAL_SERVER_ERROR;
    return false;

response:
    response_data.body_length = strlen(body);
    response_data.headers = PyObject_GetAttr(wsgi_object, PY_STRING_response_headers);
    response_data.status  = PyObject_GetAttr(wsgi_object, PY_STRING_response_status);
    Py_INCREF(response_data.headers);
    Py_INCREF(response_data.status);

    goto cleanup;

file_response:
    response_data.writer = wsgi_handler_sendfile;

cleanup:
    Py_XDECREF(wsgi_object);
    return true;
}

static response_status
wsgi_handler_write(Transaction* transaction)
{
    /* TODO: Maybe it's faster to write HTTP headers and request body at once? */
    if(!transaction->headers_sent) {
        /* First time we write to this client, send HTTP headers. */
        bjoern_wsgi_response_send_headers(transaction);
        return RESPONSE_NOT_YET_FINISHED;
    } else {
        ssize_t bytes_sent = write(
            transaction->client_fd,
            transaction->request_handler.body
            transaction->request_handler.body_length
        );
        if(bytes_sent == -1) {
            /* An error occured. */
            if(errno == EAGAIN)
                return RESPONSE_NOT_YET_FINISHED; /* Try again next time. */
            else
                DO_NOTHING; /* TODO: Machwas! */
        }
        return RESPONSE_FINISHED;
    }
}

static void
wsgi_handler_send_headers(Transaction* transaction)
{
    /* TODO: Maybe sprintf is faster than bjoern_strcpy? */

    char        header_buffer[MAX_HEADER_SIZE];
    char*       buffer_position = header_buffer;
    char*       orig_buffer_position = buffer_position;
    bool        have_content_length = false;
    PyObject*   header_tuple;

    #define BUF_CPY(s) bjoern_strcpy(&buffer_position, s)

    /* Copy the HTTP status message into the buffer: */
    BUF_CPY("HTTP/1.1 ");
    BUF_CPY(PyString_AsString(transaction->response_handler->status));
    BUF_CPY("\r\n");

    if(transaction->response_headers)
    {
        size_t header_tuple_length = PyTuple_GET_SIZE(transaction->response_handler->headers);
        for(int i=0; i<header_tuple_length; ++i)
        {
            header_tuple = PyTuple_GET_ITEM(transaction->response_handler->headers, i);
            BUF_CPY(PyString_AsString(PyTuple_GetItem(header_tuple, 0)));
            BUF_CPY(": ");
            BUF_CPY(PyString_AsString(PyTuple_GetItem(header_tuple, 1)));
            BUF_CPY("\r\n");
        }

        /* Make sure a Content-Length header is set: */
        if(!have_content_length)
            buffer_position += sprintf(
                buffer_position,
                "Content-Length: %d\r\n",
                transaction->response_handler->body_length
            );
    }

    BUF_CPY("\r\n");

    #undef BUF_CPY

    write(transaction->client_fd, header_buffer,
          buffer_position - orig_buffer_position);
}

static response_status
wsgi_handler_sendfile(Transaction* transaction)
{
    assert(0);

    GIL_LOCK();
    PyFile_IncUseCount((PyFileObject*)transaction->response_file);
    GIL_UNLOCK();

    #define __blaaaa__ sendfile(transaction->client_fd, PyFileno(transaction->response_file), \
                 /* offset, *must* be */NULL, /* TODO: Proper file size */ 0)

    GIL_LOCK();
    PyFile_DecUseCount((PyFileObject*)transaction->response_file);
    GIL_UNLOCK();

    return RESPONE_FINISHED;
}

static void
wsgi_handler_finalize(Transaction* transaction)
{
    assert(0);
}
