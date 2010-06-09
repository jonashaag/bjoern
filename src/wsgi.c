#define SERVER_ERROR do { DEBUG("Error on line %d", __LINE__); goto http_500_internal_server_error; } while(0)

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
    if(! (args1 = PyTuple_Pack_NoINCREF(/* size */ 1, transaction->request_environ)))
        SERVER_ERROR;
    if(! (wsgi_object = PyObject_CallObject(wsgi_layer, args1)))
        SERVER_ERROR;

    /* Call the WSGI application: app(environ, start_response, [**kwargs]). */
    if(! (args2 = PyTuple_Pack_NoINCREF(/* size */ 2, transaction->request_environ, wsgi_object)))
        SERVER_ERROR;

#ifdef WANT_ROUTING
    if(! (return_value = PyObject_Call(route->wsgi_callback, args2, transaction->route_kwargs)))
        SERVER_ERROR;
#else
    if(! (return_value = PyObject_CallObject(wsgi_application, args2)))
        SERVER_ERROR;
#endif

    transaction->dealloc_extra = return_value;

    /* Make sure to fetch the `_response_headers` attribute before anything else. */
    transaction->headers = PyObject_GetAttr(wsgi_object, PYSTRING(response_headers));
    if(!PyTuple_Check(transaction->headers)) {
        PyErr_Format(
            PyExc_TypeError,
            "response_headers must be a tuple, not %.200s",
            Py_TYPE(transaction->headers)->tp_name
        );
        transaction->headers = PyTuple_Pack(/* size */ 0);
        goto http_500_internal_server_error;
    }

    if(PyFile_Check(return_value)) {
        goto file_response;
    }

    if(PyString_Check(return_value)) {
        /* We already have a string. That's ok, take it for the response. */

        transaction->body_length = PyString_Size(return_value);
        goto response;
    }

    if(PySequence_Check(return_value)) {
        /* WSGI-compliant case, we have a list or tuple --
           just pick the first item of that (for now) for the response.
           FIXME. */
        PyObject* item_at_0 = PySequence_GetItem(return_value, 0);
        transaction->body = PyString_AsString(item_at_0);
        transaction->body_length = PyString_Size(item_at_0);
        goto response;
    }

    /* Not a string, not a file, no list/tuple/sequence -- TypeError */
    SERVER_ERROR;


http_500_internal_server_error:
    /* HACK: We redirect this request directly to the HTTP 500 transaction-> */
    transaction->status = PYSTRING(500_INTERNAL_SERVER_ERROR);
    assert(transaction->status);
    retval = false;
    goto cleanup;

file_response:
    transaction->use_sendfile = true;
    Py_INCREF(return_value);
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
        goto http_500_internal_server_error;
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
        transaction->body,
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
        transaction->body += bytes_sent;
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
    for(int i=0; i<header_tuple_length; ++i)
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
            "Content-Length: %d\r\n",
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

static bool
wsgi_sendfile_init(Transaction* transaction, PyFileObject* file)
{
    int file_descriptor;

    PyFile_IncUseCount(file);

    transaction->body = file;
    file_descriptor = PyObject_AsFileDescriptor((PyObject*)file);

    struct stat file_stat;
    if(fstat(file_descriptor, &file_stat) == -1) {
        /* an error occured */
        return false;
    } else {
        transaction->body_length = file_stat.st_size;

        /* Ensure the file's mime type is set. */
        if(true || !PyDict_Contains(transaction->headers, PYSTRING(Content_Type)))
        {
            const char* filename = PyString_AsString(PyFile_Name((PyObject*)file));
            const char* mimetype = get_mimetype(filename);
            if(mimetype == NULL)
                return false;
            /* the following is equivalent to the Python expression
                  headers = headers + (('Content-Type', the_mimetype),)
               hence, it concats a tuple containing 'Content-Type' as
               item 0 and the_mimetype as item 1 to the headers tuple.
            */
            PyObject* inner_tuple = PyTuple_Pack(/* size */ 2, PYSTRING(Content_Type), PyString_FromString(mimetype));
            PyObject* outer_tuple = PyTuple_Pack(/* size */ 1, inner_tuple);
            transaction->headers = PyNumber_Add(transaction->headers, outer_tuple);
            /* `PyNumber_Add` isn't restricted to numbers at all but just represents
               a Python '+'.  Fuck the Python C API! */
            return transaction->headers != NULL;
        }
        else {
            return true;
        }
    }
}

static response_status
wsgi_sendfile(Transaction* transaction)
{
    int file_descriptor;
    response_status return_value;

    GIL_LOCK();

    file_descriptor = PyObject_AsFileDescriptor(transaction->body);
    //DEBUG("Continue sendfile() , %d bytes left to send.", transaction->body_length);
    ssize_t bytes_sent = sendfile(
        transaction->client_fd,
        file_descriptor,
        NULL /* offset=NULL: let sendfile() manage the file offset */,
        transaction->body_length
    );
    //DEBUG("sendfile(): sent %d bytes.", bytes_sent);
    if(bytes_sent == -1) {
        /* socket error [note 1] */
        if(errno == EAGAIN) {
            return_value = RESPONSE_NOT_YET_FINISHED; /* Try again next time. */
            goto unlock_GIL_and_return;
        }
        else {
            return_value = RESPONSE_SOCKET_ERROR_OCCURRED;
            goto close_connection;
        }
    }

    transaction->body_length -= bytes_sent;

    if(bytes_sent == 0 || transaction->body_length == 0) {
        /* Everything sent */
        return_value = RESPONSE_FINISHED;
        goto close_connection;
    } else {
        /* There's data left to send */
        return_value = RESPONSE_NOT_YET_FINISHED;
        goto unlock_GIL_and_return;
    }

close_connection:
    PyFile_DecUseCount(transaction->body);
    Py_DECREF(transaction->body);
    goto unlock_GIL_and_return;

unlock_GIL_and_return:
    GIL_UNLOCK();
    return return_value;
}

static inline void
wsgi_finalize(Transaction* transaction)
{
    GIL_LOCK();
    Py_XDECREF(transaction->request_environ);
    Py_XDECREF(transaction->dealloc_extra);
    GIL_UNLOCK();
}
