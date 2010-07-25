#include "bjoern.h"
#include "utils.c"
#include "getmimetype.c"
#include "parsing.c"
#ifdef WANT_ROUTING
#  include "routing.c"
#endif
#include "wsgi.c"

static PyMethodDef Bjoern_FunctionTable[] = {
    {"run", Bjoern_Run, METH_VARARGS, "Run bjoern. :-)"},
#ifdef WANT_ROUTING
    {"add_route", Bjoern_Route_Add, METH_VARARGS, "Add a URL route. :-)"},
#endif
    {NULL,  NULL, 0, NULL}
};


PyMODINIT_FUNC init_bjoern()
{
    #include "stringcache.h"
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
    if(!PyArg_ParseTuple(args, "OsiO", &wsgi_application, &hostaddress, &port, &wsgi_layer))
        return NULL;
    Py_INCREF(wsgi_application);
#endif

    Py_INCREF(wsgi_layer);

    sockfd = init_socket(hostaddress, port);
    if(sockfd < 0) {
        PyErr_Format(PyExc_RuntimeError, "%s", /* error message */(char*)sockfd);
        return NULL;
    }

    EV_LOOP* mainloop = ev_loop_new(0);

    ev_io accept_watcher;
    ev_io_init(&accept_watcher, on_sock_accept, sockfd, EV_READ);
    ev_io_start(mainloop, &accept_watcher);

    ev_signal signal_watcher;
    ev_signal_init(&signal_watcher, on_sigint, SIGINT);
    ev_signal_start(mainloop, &signal_watcher);

    shall_cleanup = true;

    Py_BEGIN_ALLOW_THREADS
    ev_loop(mainloop, 0);
    Py_END_ALLOW_THREADS

    bjoern_cleanup(mainloop);

    Py_RETURN_NONE;
}

static ssize_t
init_socket(const char* hostaddress, const int port)
{
    sockfd = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
    if(sockfd < 0) return (size_t) "socket() failed";

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
    if(st < 0) return (size_t) "bind() failed";

    st = listen(sockfd, MAX_LISTEN_QUEUE_LENGTH);
    if(st < 0) return (size_t) "listen() failed";

    return sockfd;
}


static Transaction* Transaction_new()
{
    /* calloc(n, m): allocate n*m bytes of NULL-ed memory */
    Transaction* transaction = calloc(1, sizeof(Transaction));

    /* Allocate and initialize the http parser. */
    transaction->request_parser = calloc(1, sizeof(bjoern_http_parser));
    if(!transaction->request_parser) {
        free(transaction);
        return NULL;
    }
    transaction->request_parser->transaction = transaction;
    /* Reset the parser exit state */
    ((http_parser*)transaction->request_parser)->data = PARSER_OK;

    http_parser_init((http_parser*)transaction->request_parser, HTTP_REQUEST);

    return transaction;
}

/*
    Called when a new client is accepted on the socket file.
*/
static void
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
    int v = fcntl(transaction->client_fd, F_GETFL, 0);
    fcntl(transaction->client_fd, F_SETFL, (v < 0 ? 0 : v) | O_NONBLOCK);

    /* Run the read-watch loop. */
    ev_io_init(&transaction->read_watcher, &on_sock_read, transaction->client_fd, EV_READ);
    ev_io_start(mainloop, &transaction->read_watcher);
}



/*
    Called when the socket is readable, thus, an HTTP request waits to be parsed.
*/
static void
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
                &parser_settings,
                read_buffer,
                bytes_read
            );

            unsigned int exit_code = transaction->request_parser->exit_code;

            DEBUG("Parser exited with %d.", exit_code);

            switch(exit_code) {
                case PARSER_OK:
                    /* standard case, call wsgi app */
                    if(wsgi_call_app(transaction))
                        break;
                    else
                        /* error in `wsgi_call_app`, "forward" to HTTP 500 */
                case HTTP_INTERNAL_SERVER_ERROR:
                    set_http_500_response(transaction);
                    break;
                case HTTP_NOT_FOUND:
                    set_http_404_response(transaction);
                    break;
                default:
                    assert(0);
            }

            if(bjoern_check_errors(mainloop))
                set_http_500_response(transaction);
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

static void
set_http_500_response(Transaction* transaction)
{
    transaction->body = PyString_FromString(HTTP_500_MESSAGE);
    transaction->body_position = PyString_AsString(transaction->body);
    transaction->body_length = strlen(HTTP_500_MESSAGE);
    transaction->headers_sent = true; /* ^ contains the body, no additional headers needed */
}

static void
set_http_404_response(Transaction* transaction)
{
    transaction->body = PyString_FromString(HTTP_404_MESSAGE);
    transaction->body_position = PyString_AsString(transaction->body);
    transaction->body_length = strlen(HTTP_404_MESSAGE);
    transaction->headers_sent = true; /* ^ contains the body, no additional headers needed */
}


/*
    Called when the socket is writable, thus, we can send the HTTP response.
*/
static void
while_sock_canwrite(EV_LOOP* mainloop, ev_io* write_watcher_, int revents)
{
    DEBUG("Can write.");
    Transaction* transaction = OFFSETOF(write_watcher, write_watcher_, Transaction);

    switch(wsgi_send_response(transaction)) {
        case RESPONSE_NOT_YET_FINISHED:
            /* Please come around again. */
            DEBUG("Not yet finished.");
            return;
        case RESPONSE_FINISHED:
            DEBUG("Finished!");
            goto finish;
        case RESPONSE_SOCKET_ERROR_OCCURRED:
            /* occurrs e.g. if the client closed the connection before we sent all data.
               Simply do the same: Close the connection. Goodbye client! */
            DEBUG("Socket error: %d", errno);
            goto finish;
        default:
            assert(0);
    }

finish:
    ev_io_stop(mainloop, &transaction->write_watcher);
    close(transaction->client_fd);
    wsgi_finalize(transaction);
    Transaction_free(transaction);
}


/* Shutdown/cleanup stuff */
static bool
bjoern_check_errors(EV_LOOP* loop)
{
    GIL_LOCK();
    if(PyErr_Occurred()) {
        PyErr_Print();
        /*#if DIE_ON_ERROR
          bjoern_cleanup(loop);
          PyErr_SetInterrupt();
        #endif*/
        GIL_UNLOCK();
        return true;
    } else {
        GIL_UNLOCK();
        return false;
    }
}

static inline void
bjoern_cleanup(EV_LOOP* loop)
{
    if(shall_cleanup) {
        ev_unloop(loop, EVUNLOOP_ALL);
        shall_cleanup = false;
    }
}

/*
    Called if the program received a SIGINT (^C, KeyboardInterrupt, ...) signal.
*/
static void
on_sigint(EV_LOOP* loop, ev_signal* signal_watcher, const int revents)
{
    PyErr_SetInterrupt();
    bjoern_cleanup(loop);
}
