#include "bjoern.h"
#include "http.c"

/* TODO: Use global parser if possible */

static struct http_parser_settings
  parser_settings = {
    http_on_start_parsing,      /* http_cb      on_message_begin; */
    http_on_path,               /* http_data_cb on_path; */
    http_on_query,              /* http_data_cb on_query_string; */
    http_on_url,                /* http_data_cb on_url; */
    http_on_fragment,           /* http_data_cb on_fragment; */
    http_on_header_name,        /* http_data_cb on_header_field; */
    http_on_header_value,       /* http_data_cb on_header_value; */
    http_on_headers_complete,   /* http_cb      on_headers_complete; */
    http_on_body,               /* http_data_cb on_body; */
    http_on_end_parsing,        /* http_cb      on_message_complete; */
};

IF_DEBUG(static int NUMS = 1);

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

    IF_DEBUG(transaction->num = NUMS++);

    DEBUG("New transaction %d.", transaction->num);

    return transaction;
}

static void Transaction_free(TRANSACTION* transaction)
{
    /* TODO: Free PyObjects here? */
    //free(transaction->response);
    free(transaction->request_parser);
    free(transaction);
}


/*
    Start *or continue previously started* writing to the socket.
*/
static void while_sock_canwrite(EV_LOOP mainloop, ev_io* write_watcher_, int revents)
{
    int bytes_sent;

    DEBUG("Write start.");

    TRANSACTION* transaction = OFFSETOF(write_watcher, write_watcher_, TRANSACTION);

    if(transaction->response_file) {
        /* We we'll send the contents of a file, use the `sendfile` supercow */

        GIL_LOCK();
        PyFile_IncUseCount((PyFileObject*)transaction->response_file);
        GIL_UNLOCK();

        /* sendfile(output_fd[socket], input_fd[file], offset, maxlength) */
        bytes_sent = (int)sendfile(transaction->client_fd,
                                   PyFileno(transaction->response_file),
                                   NULL /* important */, WRITE_SIZE);

        GIL_LOCK();
        PyFile_DecUseCount((PyFileObject*)transaction->response_file);
        GIL_UNLOCK();
    }
    else {
        if(!transaction->headers_sent) {
            /* First time we to send to this client, send HTTP headers. */
            #define headers "HTTP/1.1 200 Alles Ok\r\n\r\nContent-Type: text/plain\r\nContent-Length: 25\r\n\r\n"
            write(transaction->client_fd, headers, strlen(headers));
            transaction->headers_sent = true;
            return;
        } else {
            DEBUG("Len: %d", strlen(transaction->response));
            /* Start or go on sending the HTTP response. */
            bytes_sent = (int)write(transaction->client_fd,
                                    transaction->response,
                                    WRITE_SIZE);
        }
    }

    switch(bytes_sent) {
        case 0:
            /* Response finished properly. Done with this request! :-) */
            goto finish;
        case -1:
            /* An error occured. */
            if(errno == EAGAIN) DO_NOTHING;
                /* Non-critical error, try again. (Write operation would block
                   but we set the socket to be nonblocking.)*/
            else goto error;
        default:
            /* we have sent `bytes_sent` bytes but we haven't sent everything
               yet, so go on sending (do not stop the write event loop). */
            DEBUG("SENT %d bytes", bytes_sent);
            return;
    }

error:
    DO_NOTHING;
finish:
    Py_XDECREF(transaction->response_file);
    ev_io_stop(mainloop, &transaction->write_watcher);
    close(transaction->client_fd);
    Transaction_free(transaction);
    DEBUG("Write end.\n\n");
}


/*
    TODO: Make sure this function is very, very fast as it is called many times.
    TODO: Split. This has about 100 LOC. :'(
*/
static void on_sock_read(EV_LOOP mainloop, ev_io* read_watcher_, int revents)
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
        goto send_response;
    }

    Py_INCREF(return_value);

    if(PyIter_Check(return_value)) goto iterators_currently_not_supported;

    if(PyFile_Check(return_value)) {
        transaction->response_file = return_value;
        goto send_response;
    }

    PyObject* response;
    if(!PyString_Check(return_value)) {
        /* WSGI-compliant case, we have a list or tuple --
           just pick the first item of that (for now) for the response. */
        response = PySequence_GetItem(return_value, 1);
        Py_INCREF(response);
        Py_DECREF(return_value);
    } else {
        /* We already have a string. That's ok, take it for the response. */
        response = return_value;
    }

    transaction->response = PyString_AsString(response);
    if(!transaction->response) goto item_wasnt_a_string_throw_typeerror;

send_response:
    ev_io_init(&transaction->write_watcher, &while_sock_canwrite,
               transaction->client_fd, EV_WRITE);
    ev_io_start(mainloop, &transaction->write_watcher);
    return;

iterators_currently_not_supported:
    PyRaise(NotImplementedError, "Cannot handle iterator return value");
    return;

item_wasnt_a_string_throw_typeerror:
    PyRaise(TypeError, "Return value item 1 not of type 'str'");
}


static void on_sock_accept(EV_LOOP mainloop, ev_io* accept_watcher, int revents)
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


static int init_socket(const char* hostaddress, const int port)
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


static void on_sigint_received(EV_LOOP mainloop, ev_signal *signal, int revents)
{
    printf("\b\bReceived SIGINT, shutting down. Goodbye!\n");
    ev_unloop(mainloop, EVUNLOOP_ALL);
}


static void* run_ev_loop()
{
    ev_loop(mainloop, 0);
    return NULL;
}

PyObject* PyB_Run(PyObject* self, PyObject* args)
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

    run_ev_loop();

    Py_RETURN_NONE;
}



static PyMethodDef Bjoern_FunctionTable[] = {
    {"run", PyB_Run, METH_VARARGS, "Run bjoern. :-)"},
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

    Py_InitModule("_bjoern", Bjoern_FunctionTable);
}
