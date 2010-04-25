#include "bjoern.h"
#include "parsing.c"

static PyMethodDef Bjoern_FunctionTable[] = {
    {"run", Bjoern_Run, METH_VARARGS, "Run bjoern. :-)"},
    {NULL,  NULL, 0, NULL}
};

PyMODINIT_FUNC init_bjoern()
{
    #define INIT_PYSTRINGS
    #include "strings.c"
    #undef  INIT_PYSTRINGS
    Py_InitModule("_bjoern", Bjoern_FunctionTable);
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
    if(sockfd < 0) return PyErr(PyExc_RuntimeError, SOCKET_ERROR_MESSAGES[-sockfd]);

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
on_sigint_received(EV_LOOP* mainloop, ev_signal *signal, int revents)
{
    printf("\b\bReceived SIGINT, shutting down. Goodbye!\n");
    ev_unloop(mainloop, EVUNLOOP_ALL);
}


static Transaction* Transaction_new()
{
    Transaction* transaction = ALLOC(sizeof(Transaction));

    /* Allocate and initialize the http parser. */
    transaction->request_parser = ALLOC(sizeof(bjoern_http_parser));
    if(!transaction->request_parser) return NULL;
    transaction->request_parser->transaction = transaction;

    http_parser_init((http_parser*)transaction->request_parser, HTTP_REQUEST);

    /* Initialize the Python headers dictionary. */
    transaction->wsgi_environ = PyDict_New();
    if(!transaction->wsgi_environ) return NULL;
    Py_INCREF(transaction->wsgi_environ);

    return transaction;
}

static ev_io_callback
on_sock_accept(EV_LOOP* mainloop, ev_io* accept_watcher, int revents)
{
    Transaction* transaction = Transaction_new();

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



/*
    TODO: Make sure this function is very, very fast as it is called many times.
*/
static ev_io_callback
on_sock_read(EV_LOOP* mainloop, ev_io* read_watcher_, int revents)
{
    /* Check whether we can still read. */
    if(!(revents & EV_READ))
        return;

    char    read_buffer[READ_BUFFER_SIZE];
    ssize_t bytes_read;
    size_t  bytes_parsed;

    Transaction* transaction = OFFSETOF(read_watcher, read_watcher_, Transaction);

    /* Read from the client: */
    bytes_read = read(transaction->client_fd, read_buffer, READ_BUFFER_SIZE);

    switch(bytes_read) {
        case 0:  goto read_finished;
        case -1: return; /* An error happened. Try again next time libev calls
                            this function. TODO: Limit retries? */
        default:
            bytes_parsed = http_parser_execute(
                (http_parser*)transaction->request_parser,
                parser_settings,
                read_buffer,
                bytes_read
            );
            DEBUG("Parsed %d bytes of %d", (int)bytes_parsed, (int)bytes_read);
    }

read_finished:
    /* Stop the read loop, we're done reading. */
    ev_io_stop(mainloop, &transaction->read_watcher);

    switch(transaction->request_handler) {
        case WSGI_APPLICATION_HANDLER:
            wsgi_request_handler(transaction);
            goto start_write;
        default:
            assert(0);
    }

start_write:
    ev_io_init(&transaction->write_watcher, &while_sock_canwrite,
               transaction->client_fd, EV_WRITE);
    ev_io_start(mainloop, &transaction->write_watcher);
}

/*
    Interact with the Python WSGI app.
*/
static bool
wsgi_request_handler(Transaction* transaction)
{
    PyObject* py_tmp;
    PyObject* wsgi_object;

    /* Create a new instance of the WSGI layer class. */
    py_tmp = Py_BuildValue("(O)", transaction->wsgi_environ);
    if(py_tmp == NULL)
        return false;
    Py_INCREF(py_tmp);

    wsgi_object = PyObject_Call(wsgi_layer, py_tmp, NULL /* kwargs */);
    Py_DECREF(py_tmp);
    py_tmp = NULL;

    if(wsgi_object == NULL)
        return false;
    Py_INCREF(wsgi_object);

    /* Call the WSGI application: app(environ, start_response). */
    PyObject* args = Py_BuildValue("OO",
        transaction->wsgi_environ,
        PyObject_GetAttr(wsgi_object, PY_STRING_start_response)
    );
    if(args == NULL)
        return false;
    Py_INCREF(args);
    PyObject* return_value = PyObject_Call(wsgi_application,
                                           args, NULL /* kwargs */);
    Py_DECREF(args);


    if(return_value == NULL) goto http_500_internal_server_error;
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
        transaction->response = PyString_AsString(return_value);
        goto string_response;
    }

    if(PySequence_Check(return_value)) {
        /* WSGI-compliant case, we have a list or tuple --
           just pick the first item of that (for now) for the response. */
        py_tmp = PySequence_GetItem(return_value, 0);
        Py_DECREF(return_value); /* don't need this anymore */
        transaction->response = PyString_AsString(py_tmp);
        goto string_response;
    }

    /* Not a string, not a file, no list/tuple/sequence -- TypeError */
    goto http_500_internal_server_error;


http_500_internal_server_error:
    transaction->response = HTTP_500_MESSAGE;

string_response:
    transaction->response_remaining = strlen(transaction->response);
    transaction->response_headers = PyGetAttr(wsgi_object, "response_headers");
    transaction->response_status  = PyGetAttr(wsgi_object, "response_status");
    Py_INCREF(transaction->response_headers);
    Py_INCREF(transaction->response_status);
    goto cleanup;

file_response:
    goto cleanup;

cleanup:
    Py_DECREF(wsgi_object);
    return true;
}



/*
    Start (or continue) writing to the socket.
*/
static ev_io_callback
while_sock_canwrite(EV_LOOP* mainloop, ev_io* write_watcher_, int revents)
{
    ssize_t bytes_sent;
    Transaction* transaction = OFFSETOF(write_watcher, write_watcher_, Transaction);

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
            if(transaction->response_remaining == 0)
                goto finish;
    }
    #undef RESPONSE_FUNC

error:
    goto finish;
finish:
    ev_io_stop(mainloop, &transaction->write_watcher);
    close(transaction->client_fd);
    Transaction_free(transaction);
}

static ssize_t
bjoern_http_response(Transaction* transaction)
{
    /* TODO: Maybe it's faster to write HTTP headers and request body at once? */
    if(!transaction->headers_sent) {
        /* First time we write to this client, send HTTP headers. */
        bjoern_send_headers(transaction);
        return -2;
    } else {
        if(transaction->response_remaining == 0)
            return 0;
        /* Start or go on sending the HTTP response. */
        return write(
            transaction->client_fd,
            transaction->response,
            transaction->response_remaining
        );
    }
}

static void
bjoern_send_headers(Transaction* transaction)
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
bjoern_sendfile(Transaction* transaction)
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
