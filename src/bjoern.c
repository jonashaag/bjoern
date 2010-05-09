#include "bjoern.h"
#include "parsing.c"
#ifdef WANT_ROUTING
  #include "routing.c"
#endif

static PyMethodDef Bjoern_FunctionTable[] = {
    {"run", Bjoern_Run, METH_VARARGS, "Run bjoern. :-)"},
#ifdef WANT_ROUTING
    {"add_route", Bjoern_Route_Add, METH_VARARGS, "Add a URL route. :-)"},
#endif
    {NULL,  NULL, 0, NULL}
};

PyMODINIT_FUNC init_bjoern()
{
    #define INIT_PYSTRINGS
    #include "strings.h"
    #undef  INIT_PYSTRINGS

    Py_InitModule("_bjoern", Bjoern_FunctionTable);

#ifdef WANT_ROUTING
    import_re_module();
    init_routing();
#endif
}


PyObject* Bjoern_Run(PyObject* self, PyObject* args)
{
    const char* hostaddress;
    const int   port;

#ifdef WANT_ROUTING
    if(!PyArg_ParseTuple(args, "siO", &hostaddress, &port, &wsgi_layer))
        return NULL;
#else
    if(!PyArg_ParseTuple(args, "OsiO",
                         &wsgi_application, &hostaddress, &port, &wsgi_layer))
        return NULL;
    Py_INCREF(wsgi_application);
#endif

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
    if(!transaction->request_parser) {
        free(transaction);
        return NULL;
    }
    transaction->request_parser->transaction = transaction;
    /* Reset the parser exit state */
    ((http_parser*)transaction->request_parser)->data = PARSER_OK;

    http_parser_init((http_parser*)transaction->request_parser, HTTP_REQUEST);

    /* Initialize the Python headers dictionary. */
    transaction->wsgi_environ = PyDict_New();
    if(!transaction->wsgi_environ) {
        Transaction_free(transaction);
        return NULL;
    }
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
        case  0: goto read_finished;
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

            switch(transaction->request_parser->exit_code) {
                case PARSER_OK:
                    set_response_from_wsgi_app(transaction);
                    goto read_finished;
#ifdef WANT_CACHING
                case USE_CACHE:
                    set_response_from_cache(transaction);
                    goto read_finished;
#endif
                case HTTP_NOT_FOUND:
                    set_response_http_404(transaction);
                    goto read_finished;
                case HTTP_INTERNAL_SERVER_ERROR:
                    set_response_http_500(transaction);
                    goto read_finished;
            }
    }

read_finished:
    /* Stop the read loop, we're done reading. */
    ev_io_stop(mainloop, &transaction->read_watcher);
    goto start_write;

start_write:
    ev_io_init(&transaction->write_watcher, &while_sock_canwrite,
               transaction->client_fd, EV_WRITE);
    ev_io_start(mainloop, &transaction->write_watcher);
}


static inline void
set_response(Transaction* transaction, PyObject* http_status, /* string! */
             PyObject* headers, const char* response, size_t response_length)
{
    transaction->response_status = http_status;
    transaction->response_headers = headers;
    transaction->response = response;
    transaction->response_remaining = response_length;
}

static void
set_response_http_500(Transaction* transaction)
{
    if(PyErr_Occurred())
        PyErr_Print();

    set_response(transaction,
        PY_STRING_500_INTERNAL_SERVER_ERROR,
        /* headers */ NULL,
        HTTP_500_MESSAGE,
        strlen(HTTP_500_MESSAGE)
    );
}

static void
set_response_http_404(Transaction* transaction)
{
    set_response(transaction,
        PY_STRING_404_NOT_FOUND,
        /* headers */ NULL,
        HTTP_404_MESSAGE,
        strlen(HTTP_404_MESSAGE)
    );
}

#ifdef WANT_CACHING
static void
set_response_from_cache(Transaction* transaction)
{
}
#endif


static void
set_response_from_wsgi_app(Transaction* transaction)
{
    PyObject* args;
    PyObject* wsgi_object;

#ifdef WANT_ROUTING
    Route* route = (Route*) ((http_parser*)transaction->request_parser)->data;
#endif

    /* Create a new instance of the WSGI layer class. */
    args = PyTuple_Pack(/* size */ 1, transaction->wsgi_environ);
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
        transaction->wsgi_environ,
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


    const char* response;

    if(PyString_Check(return_value)) {
        /* We already have a string. That's ok, take it for the response. */
        response = PyString_AsString(return_value);
        goto string_response;
    }

    if(PySequence_Check(return_value)) {
        /* WSGI-compliant case, we have a list or tuple --
           just pick the first item of that (for now) for the response. */
        response = PyString_AsString(PySequence_GetItem(return_value, 0));
        Py_DECREF(return_value); /* don't need this anymore */
        goto string_response;
    }

    /* Not a string, not a file, no list/tuple/sequence -- TypeError */
    goto http_500_internal_server_error;


http_500_internal_server_error:
    set_response_http_500(transaction);
    return;

string_response: /* keep the following semicolon */;
    PyObject* response_headers;
    PyObject* response_status;

    response_headers = PyObject_GetAttr(wsgi_object, PY_STRING_response_headers);
    response_status  = PyObject_GetAttr(wsgi_object, PY_STRING_response_status);
    Py_INCREF(response_headers);
    Py_INCREF(response_status);

    set_response(transaction,
        response_status,
        response_headers,
        response,
        strlen(response)
    );

    goto cleanup;

file_response:
    goto cleanup;

cleanup:
    Py_DECREF(wsgi_object);
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
    /* TODO: Maybe sprintf is faster than bjoern_strcpy? */

    char        header_buffer[MAX_HEADER_SIZE];
    char*       buffer_position = header_buffer;
    char*       orig_buffer_position = buffer_position;
    bool        have_content_length = false;
    PyObject*   header_tuple;

    #define BUF_CPY(s) bjoern_strcpy(&buffer_position, s)

    /* Copy the HTTP status message into the buffer: */
    BUF_CPY("HTTP/1.1 ");
    BUF_CPY(PyString_AsString(transaction->response_status));
    BUF_CPY("\r\n");

    if(transaction->response_headers)
    {
        size_t header_tuple_length = PyTuple_GET_SIZE(transaction->response_headers);
        for(int i=0; i<header_tuple_length; ++i)
        {
            header_tuple = PyTuple_GET_ITEM(transaction->response_headers, i);
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
                transaction->response_remaining
            );
    }

    BUF_CPY("\r\n");

    #undef BUF_CPY

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

    assert(0);

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
