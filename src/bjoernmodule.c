#include "bjoernmodule.h"
#include "routing.h"
#include "read.h"

static PyMethodDef Bjoern_FunctionTable[] = {
    {"run", Bjoern_Run, METH_VARARGS, "Run bjoern. :-)"},
#ifdef WANT_ROUTING
    {"add_route", Bjoern_Route_Add, METH_VARARGS, "Add a URL route. :-)"},
#endif
    {NULL,  NULL, 0, NULL}
};


PyMODINIT_FUNC init_bjoern()
{
    Py_InitModule("_bjoern", Bjoern_FunctionTable);
    staticstrings_init();

#ifdef WANT_ROUTING
    routing_init();
#endif
}

static PyObject*
Bjoern_Run(PyObject* self, PyObject* args)
{
    const char* hostaddress;
    const int   port;

#ifdef WANT_ROUTING
    if(!PyArg_ParseTuple(args, "siO", &hostaddress, &port, &response_class))
        return NULL;
#else
    if(!PyArg_ParseTuple(args, "OsiO", &wsgi_application, &hostaddress, &port, &response_class))
        return NULL;
    Py_INCREF(wsgi_application);
#endif

    Py_INCREF(response_class);

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

PyObject* Bjoern_Route_Add(PyObject* self, PyObject* args)
{
    PyObject* pattern;
    PyObject* callback;

    if(!PyArg_ParseTuple(args, "SO", &pattern, &callback))
        return NULL;

    Py_INCREF(pattern);
    Py_INCREF(callback);

    Route* new_route = Route_new(pattern, callback);
    if(!new_route)
        return NULL;
    Route_add(new_route);

    Py_RETURN_NONE;
}

/* Shutdown/cleanup stuff */
inline bool
bjoern_flush_errors()
{
    if(PyErr_Occurred()) {
        bjoern_server_error();
        /*#if DIE_ON_ERROR
          bjoern_cleanup(loop);
          PyErr_SetInterrupt();
        #endif*/
        return true;
    }
    return false;
}

inline void
bjoern_server_error()
{
    PyErr_Print();
}
