#include "bjoern.h"
#include "http.c"

/* TODO: Use global parser if possible */

static struct http_parser_settings
  parser_settings = {
    http_on_start_parsing,      /* http_cb      on_message_begin; */
    http_set_path,              /* http_data_cb on_path; */
    http_set_query,             /* http_data_cb on_query_string; */
    http_set_url,               /* http_data_cb on_url; */
    http_set_fragment,          /* http_data_cb on_fragment; */
    http_set_header,            /* http_data_cb on_header_field; */
    http_set_header_value,      /* http_data_cb on_header_value; */
    http_on_headers_complete,   /* http_cb      on_headers_complete; */
    http_set_body,              /* http_data_cb on_body; */
    http_on_end_parsing,        /* http_cb      on_message_complete; */
};

static int NUMS = 1;

static TRANSACTION* Transaction_new()
{
    TRANSACTION* transaction = ALLOC(sizeof(TRANSACTION));

    /* Allocate and initialize the http parser. */
    transaction->request_parser = ALLOC(sizeof(BJPARSER));
    if(!transaction->request_parser) return NULL;
    transaction->request_parser->transaction = transaction;

    http_parser_init((PARSER*)transaction->request_parser, HTTP_REQUEST);

    /* Initialize the Python headers dictionary. */
    transaction->request_headers = PyDict_New();
    if(!transaction->request_headers) return NULL;
    Py_INCREF(transaction->request_headers);

    transaction->num = NUMS++;

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


static void while_sock_canwrite(EV_LOOP mainloop, ev_io* write_watcher_, int revents)
{
    DEBUG("Write start.");

    TRANSACTION* transaction = OFFSETOF(write_watcher, write_watcher_, TRANSACTION);

    write(transaction->client_fd, transaction->response, WRITE_SIZE);

    ev_io_stop(mainloop, &transaction->write_watcher);
    close(transaction->client_fd);
    Transaction_free(transaction);

    DEBUG("Write end.\n\n");
}

static void callme(TRANSACTION* transaction)
{
    /* Call the Python callback function. */
    PyObject* response = PyObject_CallObject(request_callback, Py_BuildValue(
        "(ssssOs)",
        transaction->request_url,
        transaction->request_query,
        transaction->request_url_fragment,
        transaction->request_path,
        transaction->request_headers,
        transaction->request_body
    ));

    transaction->response = (const char*)PyString_AsString(response);
}

/*
    TODO: Make sure this function is very, very fast as it is called many times.
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

    callme(transaction);

    /* Run the write-watch loop that notifies us when we can write to the socket. */
    ev_io_init(&transaction->write_watcher, &while_sock_canwrite, transaction->client_fd, EV_WRITE);
    ev_io_start(mainloop, &transaction->write_watcher);
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

    Py_XDECREF(request_callback);
    if(!PyArg_ParseTuple(args, "siO", &hostaddress, &port, &request_callback))
        return NULL;

    Py_INCREF(request_callback);

    sockfd = init_socket(hostaddress, port);
    if(sockfd < 0) return PyB_Err(PyExc_RuntimeError, socket_error_format(sockfd));

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

PyMODINIT_FUNC initbjoern()
{
    Py_InitModule("bjoern", Bjoern_FunctionTable);
}
