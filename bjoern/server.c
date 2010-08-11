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

static int          sockfd;

typedef void ev_io_callback(struct ev_loop*, ev_io*, const int);
static ev_io_callback ev_io_on_request;
static ev_io_callback ev_io_on_read;
static ev_io_callback ev_io_on_write;
static bool server_init(const char* hostaddr, const int port);
static void print_io_error();
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

    /*ev_signal signal_watcher;
    ev_signal_init(&signal_watcher, ev_io_on_sigint, SIGINT);
    ev_signal_start(mainloop, &signal_watcher);*/

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

    /* Error? */
    if(read_bytes == -1) {
        if(errno & EAGAIN || errno & EWOULDBLOCK) {
            /* try again. */
            return;
        } else {
            print_io_error();
            return send_error(mainloop, request, HTTP_SERVER_ERROR);
        }
    }

    /* Finished? */
    if(read_bytes == 0) {
        if(!request->headers)
            return send_error(mainloop, request, HTTP_BAD_REQUEST);
        if(!wsgi_call_application(request))
            return send_error(mainloop, request, HTTP_SERVER_ERROR);
    } else {
        if(Request_parse(request, read_buf, read_bytes)) {
            /* parsing succeeded, wait for more data */
            return;
        } else {
            /* data could not be parsed */
            return send_error(mainloop, request, HTTP_BAD_REQUEST);
        }
    }

    ev_io_stop(mainloop, &request->ev_watcher);
    ev_io_init(&request->ev_watcher, &ev_io_on_write,
               request->client_fd, EV_WRITE);
    ev_io_start(mainloop, &request->ev_watcher);
}

static void
ev_io_on_write(struct ev_loop* mainloop, ev_io* watcher, const int events)
{
    Request* request = ADDR_FROM_MEMBER(watcher, Request, ev_watcher);
    if(wsgi_send_response(request)) {
        /* if wsgi_send_response returns true, we're done */
        ev_io_stop(mainloop, &request->ev_watcher);
        close(request->client_fd);
        Request_free(request);
    }
}

void
send_error(struct ev_loop* mainloop, Request* request, http_status status)
{
    Request_write_response(request, "HTTP/1.0 500 Error Message Here", 0);
    ev_io_stop(mainloop, &request->ev_watcher);
    close(request->client_fd);
    Request_free(request);
}

void
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
