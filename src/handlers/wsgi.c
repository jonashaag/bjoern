#define WSGI_HANDLER transaction->handler_data.wsgi
#define PYOBJ_GET_OR_HTTP_500(var, stmt) \
            do { \
                if(!(var = stmt)) \
                    goto http_500_internal_server_error; \
            } while(0)

static bool
wsgi_handler_initialize(Transaction* transaction)
{
    bool retval = true;
    PyObject* args1 = NULL;
    PyObject* args2 = NULL;
    PyObject* return_value = NULL;
    PyObject* wsgi_object = NULL;

#ifdef WANT_ROUTING
    Route* route = WSGI_HANDLER.route;
#endif

    /* Create a new instance of the WSGI layer class. */
    PYOBJ_GET_OR_HTTP_500(args1,
        PyTuple_Pack_NoINCREF(/* size */ 1, WSGI_HANDLER.request_environ));
    PYOBJ_GET_OR_HTTP_500(wsgi_object, PyObject_CallObject(wsgi_layer, args1));

    /* Call the WSGI application: app(environ, start_response). */
    PYOBJ_GET_OR_HTTP_500(args2,
        PyTuple_Pack_NoINCREF(/* size */ 2, WSGI_HANDLER.request_environ,
                              PyObject_GetAttr(wsgi_object, PYSTRING(start_response))));

#ifdef WANT_ROUTING
    PYOBJ_GET_OR_HTTP_500(return_value,
        PyObject_CallObject(route->wsgi_callback, args2)
    );
#else
    PYOBJ_GET_OR_HTTP_500(return_value,
        PyObject_CallObject(wsgi_application, args2)
    );
#endif

    WSGI_HANDLER.dealloc_extra = return_value;

    if(PyIter_Check(return_value)) {
        goto http_500_internal_server_error;
    }

    if(PyFile_Check(return_value)) {
        WSGI_HANDLER.file = return_value;
        goto file_response;
    }


    if(PyString_Check(return_value)) {
        /* We already have a string. That's ok, take it for the response. */
        WSGI_HANDLER.body = PyString_AsString(return_value);
        goto response;
    }

    if(PySequence_Check(return_value)) {
        /* WSGI-compliant case, we have a list or tuple --
           just pick the first item of that (for now) for the response.
           FIXME. */
        WSGI_HANDLER.body = PyString_AsString(PySequence_GetItem(return_value, 0));
        goto response;
    }

    /* Not a string, not a file, no list/tuple/sequence -- TypeError */
    goto http_500_internal_server_error;


http_500_internal_server_error:
    /* HACK: We redirect this request directly to the HTTP 500 WSGI_HANDLER. */
    transaction->request_parser->exit_code = HTTP_INTERNAL_SERVER_ERROR;
    retval = false;
    goto cleanup;

file_response:
    transaction->handler_write = wsgi_handler_sendfile;
    goto cleanup;

response:
    WSGI_HANDLER.body_length = strlen(WSGI_HANDLER.body);
    PYOBJ_GET_OR_HTTP_500(
        WSGI_HANDLER.headers,
        PyObject_GetAttr(wsgi_object, PYSTRING(response_headers))
    );
    PYOBJ_GET_OR_HTTP_500(
        WSGI_HANDLER.status,
        PyObject_GetAttr(wsgi_object, PYSTRING(response_status))
    );
    goto cleanup;

cleanup:
    Py_XDECREF(args1);
    Py_XDECREF(args2);
    Py_XDECREF(wsgi_object);
    return retval;
}

static response_status
wsgi_handler_write(Transaction* transaction)
{
    /* TODO: Maybe it's faster to write HTTP headers and request body at once? */
    if(!WSGI_HANDLER.headers_sent) {
        /* First time we write to this client, send HTTP headers. */
        wsgi_handler_send_headers(transaction);
        WSGI_HANDLER.headers_sent = true;
        return RESPONSE_NOT_YET_FINISHED;
    } else {
        ssize_t bytes_sent = write(
            transaction->client_fd,
            WSGI_HANDLER.body,
            WSGI_HANDLER.body_length
        );
        if(bytes_sent == -1) {
            /* An error occured. */
            if(errno == EAGAIN)
                return RESPONSE_NOT_YET_FINISHED; /* Try again next time. */
            else
                ; /* TODO: Machwas! */
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
    BUF_CPY(PyString_AsString(WSGI_HANDLER.status));
    BUF_CPY("\r\n");

    if(WSGI_HANDLER.headers)
    {
        size_t header_tuple_length = PyTuple_GET_SIZE(WSGI_HANDLER.headers);
        for(int i=0; i<header_tuple_length; ++i)
        {
            header_tuple = PyTuple_GET_ITEM(WSGI_HANDLER.headers, i);
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
                WSGI_HANDLER.body_length
            );
    }

    BUF_CPY("\r\n");

    #undef BUF_CPY

    write(transaction->client_fd, header_buffer,
          buffer_position - orig_buffer_position);

    Py_DECREF(WSGI_HANDLER.headers);
    Py_DECREF(WSGI_HANDLER.status);
}

static response_status
wsgi_handler_sendfile(Transaction* transaction)
{
    assert(0);

    PyGILState_STATE GILState;

    GIL_LOCK(GILState);
    PyFile_IncUseCount((PyFileObject*)WSGI_HANDLER.file);
    GIL_UNLOCK(GILState);

    /* sendfile(...) */

    GIL_LOCK(GILState);
    PyFile_DecUseCount((PyFileObject*)WSGI_HANDLER.file);
    GIL_UNLOCK(GILState);

    Py_DECREF(WSGI_HANDLER.file);

    return RESPONSE_FINISHED;
}

static void
wsgi_handler_finalize(Transaction* transaction)
{
    Py_DECREF(WSGI_HANDLER.request_environ);
    Py_DECREF(WSGI_HANDLER.dealloc_extra);
}

#undef WSGI_HANDLER
