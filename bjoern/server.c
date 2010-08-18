#include <sys/socket.h>
#include <sys/signal.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include "common.h"
#include "wsgi.h"
#include "server.h"

#define LISTEN_BACKLOG  1024
#define READ_BUFFER_SIZE 1024*8

#define HANDLE_IO_ERROR(n, on_fatal_err) \
    if(n == -1 || n == 0) { \
        if(n & EAGAIN) \
            goto again; \
        \
        print_io_error(); \
        on_fatal_err; \
    }


static int sockfd;

typedef void ev_signal_callback(struct ev_loop*, ev_signal*, const int);
typedef void ev_io_callback(struct ev_loop*, ev_io*, const int);
static ev_signal_callback ev_signal_on_sigint;
static ev_io_callback ev_io_on_request;
static ev_io_callback ev_io_on_read;
static ev_io_callback ev_io_on_write;
static bool server_init(const char* hostaddr, const int port);
static inline void set_error(Request*, http_status);
static inline void print_io_error();
static inline bool set_nonblocking(int fd);

bool
server_run(const char* hostaddr, const int port)
{
    if(!server_init(hostaddr, port))
        return false;

    struct ev_loop* mainloop = ev_loop_new(0);

    ev_io accept_watcher;
    ev_io_init(&accept_watcher, ev_io_on_request, sockfd, EV_READ);
    ev_io_start(mainloop, &accept_watcher);

    ev_signal signal_watcher;
    ev_signal_init(&signal_watcher, ev_signal_on_sigint, SIGINT);
    ev_signal_start(mainloop, &signal_watcher);

    /* This is the program main loop */
    Py_BEGIN_ALLOW_THREADS
    ev_loop(mainloop, 0);
    Py_END_ALLOW_THREADS

    return true;
}

static bool
server_init(const char* hostaddr, const int port)
{
    if((sockfd = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0)
        return false;

    struct sockaddr_in sockaddr;
    sockaddr.sin_family = PF_INET;
    inet_pton(AF_INET, hostaddr, &sockaddr.sin_addr);
    sockaddr.sin_port = htons(port);

    /* Set SO_REUSEADDR t make the IP address available for reuse */
    int optval = true;
    setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));

    if(bind(sockfd, (struct sockaddr*)&sockaddr, sizeof(sockaddr)) < 0)
        return false;

    if(listen(sockfd, LISTEN_BACKLOG) < 0)
        return false;

    return true;
}

static void
ev_io_on_request(struct ev_loop* mainloop, ev_io* watcher, const int events)
{
    int client_fd;
    if((client_fd = accept(watcher->fd, NULL, NULL)) < 0)
        return print_io_error();

    if(!set_nonblocking(client_fd))
        return print_io_error();

    Request* request = Request_new(client_fd);

    ev_io_init(&request->ev_watcher, &ev_io_on_read, client_fd, EV_READ);
    ev_io_start(mainloop, &request->ev_watcher);
}

static void
ev_io_on_read(struct ev_loop* mainloop, ev_io* watcher, const int events)
{
    char read_buf[READ_BUFFER_SIZE];
    Request* request = ADDR_FROM_MEMBER(watcher, Request, ev_watcher);
    ssize_t read_bytes = read(request->client_fd, read_buf, READ_BUFFER_SIZE);

    request->state = REQUEST_READING;

    GIL_LOCK(0);

    HANDLE_IO_ERROR(read_bytes,
        /* on fatal error */ set_error(request, HTTP_SERVER_ERROR); goto out
    );

    Request_parse(request, read_buf, read_bytes);

    switch(request->state) {

        case REQUEST_PARSE_ERROR:
            set_error(request, HTTP_BAD_REQUEST);
            break;
        case REQUEST_PARSE_DONE:
            if(!wsgi_call_application(request)) {
                assert(PyErr_Occurred());
                PyErr_Print();
                set_error(request, HTTP_SERVER_ERROR);
            }
            break;
        default:
            assert(request->state == REQUEST_READING);
            goto again;
    }

out:
    ev_io_stop(mainloop, &request->ev_watcher);
    ev_io_init(&request->ev_watcher, &ev_io_on_write,
               request->client_fd, EV_WRITE);
    ev_io_start(mainloop, &request->ev_watcher);

again:
    GIL_UNLOCK(0);
    return;
}

static void
ev_io_on_write(struct ev_loop* mainloop, ev_io* watcher, const int events)
{
    Request* request = ADDR_FROM_MEMBER(watcher, Request, ev_watcher);
    if(request->state > REQUEST_WSGI_GENERAL_RESPONSE) {
        /* request->response is something that the WSGI application returned */
        if(!wsgi_send_response(request))
            return; /* come around again */
    } else {
        /* request->response is a C-string */
        sendall(request, request->response, strlen(request->response));
    }

    /* Everything done, bye client! */
    ev_io_stop(mainloop, &request->ev_watcher);
    close(request->client_fd);
    Request_free(request);
}

bool
sendall(Request* request, register const char* data, register size_t length)
{
again:
    while(length) {
        ssize_t sent = write(request->client_fd, data, length);
        HANDLE_IO_ERROR(sent, /* on fatal error */ return false);
        data += sent;
        length -= sent;
    }
    return true;
}

static inline void
set_error(Request* request, http_status status)
{
    printf("Request %ld: error %d\n", request, status);
    request->response = "HTTP/1.0 500 Error msg here";
}

static inline void
print_io_error()
{
    printf("IO error %d\n", errno);
}

static inline bool
set_nonblocking(int fd)
{
    int flags = fcntl(fd, F_GETFL, 0);
    return (fcntl(fd, F_SETFL, (flags < 0 ? 0 : flags) | O_NONBLOCK) != -1);
}

static void
ev_signal_on_sigint(struct ev_loop* mainloop, ev_signal* watcher, const int events)
{
    ev_unloop(mainloop, EVUNLOOP_ALL);
}
