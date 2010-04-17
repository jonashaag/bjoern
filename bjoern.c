#include "bjoern.h"
#include "http.c"

/* TODO: Use global parser if possible */

static TRANSACTION* Transaction_new()
{
    TRANSACTION* transaction = ALLOC(sizeof(TRANSACTION));

    /* Allocate and initialize the http parser. */
    transaction->request_parser = ALLOC(sizeof(BJPARSER));
    if(!transaction->request_parser) return NULL;
    transaction->request_parser->transaction = transaction;

    http_parser_init((PARSER*)transaction->request_parser, HTTP_REQUEST);

    /* Initialize the Python headers dictionary. */
    transaction->wsgi_environ = PyDict_New();
    if(!transaction->wsgi_environ) return NULL;
    Py_INCREF(transaction->wsgi_environ);

    return transaction;
}


static ssize_t
bjoern_sendfile(TRANSACTION* transaction)
{
    ssize_t bytes_sent;

    GIL_LOCK();
    PyFile_IncUseCount((PyFileObject*)transaction->response_file);
    GIL_UNLOCK();

    #define SENDFILE \
        sendfile(transaction->client_fd, PyFileno(transaction->response_file), \
                 /* offset, *must* be */NULL, /* TODO: Proper file size */ 0)
    bytes_sent = (SENDFILE == 0); // not sure about that
    #undef SENDFILE

    GIL_LOCK();
    PyFile_DecUseCount((PyFileObject*)transaction->response_file);
    GIL_UNLOCK();

    return bytes_sent;
}

static void
bjoern_send_headers(TRANSACTION* transaction)
{
    char        header_buffer[MAX_HEADER_SIZE];
    char*       buffer_position = header_buffer;
    char*       orig_buffer_position = buffer_position;
    PyObject*   current_key;
    PyObject*   current_value;
    Py_ssize_t  dict_position = 0;

    /* Copy the HTTP status message into the buffer: */
    bjoern_strcpy(&buffer_position, "HTTP/1.1 ");
    bjoern_strcpy(&buffer_position, PyString_AsString(transaction->response_status));
    bjoern_strcpy(&buffer_position, "\r\n");

    /* Make sure a Content-Type header is set: */
    if(!PyDict_Contains(transaction->response_headers, PY_STRING_Content_Type))
    {
        PyDict_SetItem(
            transaction->response_headers,
            PY_STRING_Content_Type,
            PY_STRING_DEFAULT_RESPONSE_CONTENT_TYPE
        );
    }

    /* Make sure a Content-Length header is set: */
    if(!PyDict_Contains(transaction->response_headers, PY_STRING_Content_Length)) {
        PyObject* py_content_length = _PyInt_Format(
            (PyIntObject*)PyInt_FromSize_t(transaction->response_remaining),
            /* base */10, /* newstyle? */0
        );
        Py_INCREF(py_content_length);
        PyDict_SetItem(
            transaction->response_headers,
            PY_STRING_Content_Length,
            py_content_length
        );
        Py_DECREF(py_content_length);
    }

    while(PyDict_Next(transaction->response_headers, &dict_position,
                      &current_key, &current_value))
    {
        bjoern_strcpy(&buffer_position, PyString_AsString(current_key));
        bjoern_strcpy(&buffer_position, ": ");
        bjoern_strcpy(&buffer_position, PyString_AsString(current_value));
        bjoern_strcpy(&buffer_position, "\r\n");
    }
    bjoern_strcpy(&buffer_position, "\r\n");

    write(transaction->client_fd, header_buffer,
          buffer_position - orig_buffer_position);
}

static ssize_t
bjoern_http_response(TRANSACTION* transaction)
{
    /* TODO: Maybe it's faster to write HTTP headers and request body at once? */
    if(!transaction->headers_sent) {
        /* First time we write to this client, send HTTP headers. */
        bjoern_send_headers(transaction);
        return -2;
    } else {
        if(!transaction->response_remaining) return 0;
        /* Start or go on sending the HTTP response. */
        return write(
            transaction->client_fd,
            transaction->response,
            transaction->response_remaining
        );
    }
}

/*
    Start *or continue previously started* writing to the socket.
*/
static ev_io_callback
while_sock_canwrite(EV_LOOP mainloop, ev_io* write_watcher_, int revents)
{
    ssize_t bytes_sent;
    TRANSACTION* transaction = OFFSETOF(write_watcher, write_watcher_, TRANSACTION);

    #define RESPONSE_FUNC (transaction->response_file ? bjoern_sendfile \
                                                      : bjoern_http_response)
    switch(bytes_sent = RESPONSE_FUNC(transaction)) {
        case -1:
            /* An error occured. */
            if(errno == EAGAIN)
                /* Try again next time libev calls this function on EAGAIN. */
                return;
            else goto error;
        case -2:
            /* HTTP response headers were sent. Do nothing. */
            transaction->headers_sent = true;
            return;
        default:
            /* `bytes_sent` bytes were sent to the client, calculate the
                remaining bytes and finish on 0. */
            transaction->response_remaining -= bytes_sent;
            if(transaction->response_remaining == 0) goto finish;
    }
    #undef RESPONSE_FUNC

error:
    DO_NOTHING;
finish:
    ev_io_stop(mainloop, &transaction->write_watcher);
    close(transaction->client_fd);
    Transaction_free(transaction);
}


/*
    TODO: Make sure this function is very, very fast as it is called many times.
    TODO: Split. This has about 100 LOC. :'(
*/
static ev_io_callback
on_sock_read(EV_LOOP mainloop, ev_io* read_watcher_, int revents)
{
    /* Check whether we can still read. */
    if(!(revents & EV_READ))
        return;

    TRANSACTION* transaction = OFFSETOF(read_watcher, read_watcher_, TRANSACTION);

    /* Read the whole body. */
    char    read_buffer[READ_BUFFER_SIZE];
    ssize_t bytes_read;
    size_t  bytes_parsed;
    bytes_read = read(transaction->client_fd, read_buffer, READ_BUFFER_SIZE);

    if(bytes_read < 0)
        /* An error happened. TODO: Limit retries? */
        return;

    if(bytes_read == 0) goto read_finished;
    else {
        bytes_parsed = http_parser_execute(
            (PARSER*)transaction->request_parser,
            parser_settings,
            read_buffer,
            bytes_read
        );

        DEBUG("Parsed %d bytes of %d", (int)bytes_parsed, (int)bytes_read);
    }

read_finished:
    /* Stop the read loop, we're done reading. */
    ev_io_stop(mainloop, &transaction->read_watcher);

    /* Create a new instance of the WSGI layer class. */
    PyObject* wsgi_layer_instance = PyObject_Call(wsgi_layer,
        Py_BuildValue("(O)", transaction->wsgi_environ), NULL /* kwargs */);

    if(!wsgi_layer_instance) {
        if(PyErr_Occurred()) PyErr_Print();
        return;
    }
    Py_INCREF(wsgi_layer_instance);

    /* Call the WSGI application: app(environ, start_response). */
    PyObject* args = Py_BuildValue("OO", transaction->wsgi_environ,
                                         PyGetAttr(wsgi_layer_instance, "start_response"));
    Py_INCREF(args);
    PyObject* return_value = PyObject_Call(wsgi_application,
                                           args, NULL /* kwargs */);
    Py_DECREF(args);


    if(!return_value) {
        /* Application failed, send Internal Server Error */
        transaction->response = HTTP_500_MESSAGE;
        goto string_response;
    }

    Py_INCREF(return_value);

    if(PyIter_Check(return_value)) {
        transaction->response = HTTP_500_MESSAGE;
        goto string_response;
    }

    if(PyFile_Check(return_value)) {
        transaction->response_file = return_value;
        /* TODO: Use proper file size for `sendfile`. */
        goto file_response;
    }

    if(PyString_Check(return_value)) {
        /* We already have a string. That's ok, take it for the response. */
        transaction->response = PyString_AsString(return_value);
        goto string_response;
    }

    if(PySequence_Check(return_value)) {
        /* WSGI-compliant case, we have a list or tuple --
           just pick the first item of that (for now) for the response. */
        PyObject* py_tmp = PySequence_GetItem(return_value, 0);
        Py_DECREF(return_value);
        transaction->response = PyString_AsString(py_tmp);
        goto string_response;
    }

    /* Not a string, not a file, not a list/tuple/sequence -- TypeError */
    transaction->response = HTTP_500_MESSAGE;
    goto string_response;


string_response:
    if(!transaction->response) transaction->response = HTTP_500_MESSAGE;
    transaction->response_remaining = strlen(transaction->response);
    transaction->response_headers = PyGetAttr(wsgi_layer_instance, "response_headers");
    transaction->response_status = PyGetAttr(wsgi_layer_instance, "response_status");
    Py_INCREF(transaction->response_headers);
    Py_INCREF(transaction->response_status);
    goto send_response;

file_response:
    goto send_response;

send_response:
    Py_XDECREF(wsgi_layer_instance);
    ev_io_init(&transaction->write_watcher, &while_sock_canwrite,
               transaction->client_fd, EV_WRITE);
    ev_io_start(mainloop, &transaction->write_watcher);
}


static ev_io_callback
on_sock_accept(EV_LOOP mainloop, ev_io* accept_watcher, int revents)
{
    TRANSACTION* transaction = Transaction_new();

    /* Accept the socket connection. */
    struct sockaddr_in client_address;
    socklen_t client_address_length = sizeof(client_address);
    transaction->client_fd = accept(
        accept_watcher->fd,
        (struct sockaddr *)&client_address,
        &client_address_length
    );

    if(transaction->client_fd < 0) return; /* ERROR!1!1! */

    DEBUG("Accepted client on %d.", transaction->client_fd);

    /* Set the file descriptor non-blocking. */
    fcntl(transaction->client_fd, F_SETFL,
          MAX(fcntl(transaction->client_fd, F_GETFL, 0), 0) | O_NONBLOCK);

    /* Run the read-watch loop. */
    ev_io_init(&transaction->read_watcher, &on_sock_read, transaction->client_fd, EV_READ);
    ev_io_start(mainloop, &transaction->read_watcher);
}


static int
init_socket(const char* hostaddress, const int port)
{
    sockfd = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
    if(sockfd < 0) return SOCKET_FAILED;

    DEBUG("sockfd is %d", sockfd);

    union _sockaddress {
        struct sockaddr    sockaddress;
        struct sockaddr_in sockaddress_in;
    } server_address;

    server_address.sockaddress_in.sin_family = PF_INET;
    inet_aton(hostaddress, &server_address.sockaddress_in.sin_addr);
    server_address.sockaddress_in.sin_port = htons(port);

    /* Set SO_REUSEADDR to make the IP address we used available for reuse. */
    int optval = true;
    setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));

    int st;
    st = bind(sockfd, &server_address.sockaddress, sizeof(server_address));
    if(st < 0) return BIND_FAILED;

    st = listen(sockfd, MAX_LISTEN_QUEUE_LENGTH);
    if(st < 0) return LISTEN_FAILED;

    return sockfd;
}


static ev_signal_callback
on_sigint_received(EV_LOOP mainloop, ev_signal *signal, int revents)
{
    printf("\b\bReceived SIGINT, shutting down. Goodbye!\n");
    ev_unloop(mainloop, EVUNLOOP_ALL);
}


PyObject* Bjoern_Run(PyObject* self, PyObject* args)
{
    const char* hostaddress;
    const int   port;

    if(!PyArg_ParseTuple(args, "OsiO",
                         &wsgi_application, &hostaddress, &port, &wsgi_layer))
        return NULL;
    Py_INCREF(wsgi_application);
    Py_INCREF(wsgi_layer);

    sockfd = init_socket(hostaddress, port);
    if(sockfd < 0) return PyErr(PyExc_RuntimeError, socket_error_format(sockfd));

    mainloop = ev_loop_new(0);

    /* Run the SIGINT-watch loop. */
    ev_signal sigint_watcher;
    ev_signal_init(&sigint_watcher, &on_sigint_received, SIGINT);
    ev_signal_start(mainloop, &sigint_watcher);


    /* Run the accept-watch loop. */
    ev_io accept_watcher;
    ev_io_init(&accept_watcher, on_sock_accept, sockfd, EV_READ);
    ev_io_start(mainloop, &accept_watcher);

    ev_loop(mainloop, 0);

    Py_RETURN_NONE;
}



static PyMethodDef Bjoern_FunctionTable[] = {
    {"run", Bjoern_Run, METH_VARARGS, "Run bjoern. :-)"},
    {NULL,  NULL, 0, NULL}
};

PyMODINIT_FUNC init_bjoern()
{
    /* Py_INCREF predefined objects. */
    #define INIT_PYSTRING(s) PY_STRING_##s = PyString(#s); Py_INCREF(PY_STRING_##s)
    INIT_PYSTRING(   GET                 );
    INIT_PYSTRING(   POST                );
    INIT_PYSTRING(   REQUEST_METHOD      );
    INIT_PYSTRING(   PATH_INFO           );
    INIT_PYSTRING(   QUERY_STRING        );
    INIT_PYSTRING(   HTTP_CONTENT_TYPE   );
    INIT_PYSTRING(   CONTENT_TYPE        );
    INIT_PYSTRING(   SERVER_NAME         );
    INIT_PYSTRING(   SERVER_PORT         );
    INIT_PYSTRING(   SERVER_PROTOCOL     );
    PY_STRING_Content_Type = PyString("Content-Type");
    Py_INCREF(PY_STRING_Content_Type);
    PY_STRING_Content_Length = PyString("Content-Length");
    Py_INCREF(PY_STRING_Content_Length);
    PY_STRING_DEFAULT_RESPONSE_CONTENT_TYPE = PyString(DEFAULT_RESPONSE_CONTENT_TYPE);
    Py_INCREF(PY_STRING_DEFAULT_RESPONSE_CONTENT_TYPE);

    Py_InitModule("_bjoern", Bjoern_FunctionTable);
}
