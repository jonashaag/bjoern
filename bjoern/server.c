/**
 * server.c: Server functionality.
 * Copyright (c) 2010 Jonas Haag <jonas@lophus.org> and contributors.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#ifdef WANT_SIGINT_HANDLING
# include <sys/signal.h>
#endif
#include <ev.h>
#include "common.h"
#include "wsgi.h"
#include "server.h"

#define LISTEN_BACKLOG  1024
#define READ_BUFFER_SIZE 1024*8

static int sockfd;

typedef void ev_io_callback(struct ev_loop*, ev_io*, const int);
#if WANT_SIGINT_HANDLING
typedef void ev_signal_callback(struct ev_loop*, ev_signal*, const int);
static ev_signal_callback ev_signal_on_sigint;
#endif
static ev_io_callback ev_io_on_request;
static ev_io_callback ev_io_on_read;
static ev_io_callback ev_io_on_write;
static inline void set_error(Request*, http_status);
static inline bool set_nonblocking(int fd);
static bool send_chunk(Request*);

void
server_run(const char* hostaddr, const int port)
{
    struct ev_loop* mainloop = ev_default_loop(0);

    ev_io accept_watcher;
    ev_io_init(&accept_watcher, ev_io_on_request, sockfd, EV_READ);
    ev_io_start(mainloop, &accept_watcher);

#if WANT_SIGINT_HANDLING
    ev_signal signal_watcher;
    ev_signal_init(&signal_watcher, ev_signal_on_sigint, SIGINT);
    ev_signal_start(mainloop, &signal_watcher);
#endif

    /* This is the program main loop */
    Py_BEGIN_ALLOW_THREADS
    ev_loop(mainloop, 0);
    Py_END_ALLOW_THREADS
}

bool
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

    DBG("Listening on %s:%d...", hostaddr, port);
    return true;
}

static void
ev_io_on_request(struct ev_loop* mainloop, ev_io* watcher, const int events)
{
    int client_fd;
    if((client_fd = accept(watcher->fd, NULL, NULL)) < 0) {
        DBG("Could not accept() client: errno %d", errno);
        return;
    }

    if(!set_nonblocking(client_fd)) {
        DBG("Could not set_nonnblocking() client: errno %d", errno);
        return;
    }

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

    DBG_REQ(request, "read %zd bytes", read_bytes);

    GIL_LOCK(0);

    if(read_bytes == -1) {
        if(errno != EAGAIN && errno != EWOULDBLOCK) {
            close(request->client_fd);
            Request_free(request);
            ev_io_stop(mainloop, &request->ev_watcher);
        }
        goto out;
    }

    Request_parse(request, read_buf, read_bytes);

    if(request->state.error) {
        DBG_REQ(request, "Parse error");
        set_error(request, request->state.error_code);
    }
    else if(request->state.parse_finished) {
        DBG_REQ(request, "Parse done");
        if(!wsgi_call_application(request)) {
            assert(PyErr_Occurred());
            PyErr_Print();
            set_error(request, HTTP_SERVER_ERROR);
        }
    } else {
        DBG_REQ(request, "Waiting for more data");
        goto out;
    }

    ev_io_stop(mainloop, &request->ev_watcher);
    ev_io_init(&request->ev_watcher, &ev_io_on_write,
               request->client_fd, EV_WRITE);
    ev_io_start(mainloop, &request->ev_watcher);

out:
    GIL_UNLOCK(0);
    return;
}

static void
ev_io_on_write(struct ev_loop* mainloop, ev_io* watcher, const int events)
{
    GIL_LOCK(0);

    Request* request = ADDR_FROM_MEMBER(watcher, Request, ev_watcher);
    assert(request->current_chunk);

    if(send_chunk(request))
        goto notfinished;

    if(request->iterable) {
        PyObject* next_chunk;
        DBG_REQ(request, "Getting next iterable chunk.");
        next_chunk = wsgi_iterable_get_next_chunk(request);
        if(next_chunk == NULL) {
            if(PyErr_Occurred()) {
                /* Internal server error. */
                PyErr_Print();
                set_error(request, HTTP_SERVER_ERROR);
                goto notfinished;
            }
            DBG_REQ(request, "Iterable exhausted");
            goto bye;
        }
        request->current_chunk = next_chunk;
        assert(request->current_chunk_p == 0);
        goto notfinished;
    }

bye:
    DBG_REQ(request, "Done");

    /* Everything done, bye client! */
    ev_io_stop(mainloop, &request->ev_watcher);
    close(request->client_fd);
    Request_free(request);

notfinished:
    GIL_UNLOCK(0);
}

static bool
send_chunk(Request* request)
{
    ssize_t sent_bytes;

    assert(request->current_chunk != NULL);
    assert(request->current_chunk_p != PyString_GET_SIZE(request->current_chunk));

    DBG_REQ(request, "Sending next chunk");
    sent_bytes = write(
        request->client_fd,
        PyString_AS_STRING(request->current_chunk) + request->current_chunk_p,
        PyString_GET_SIZE(request->current_chunk) - request->current_chunk_p
    );

    if(sent_bytes == -1) {
        if(errno == EAGAIN || errno == EWOULDBLOCK) {
            /* Try again later */
            return 1;
        } else {
            /* Serious transmission failure. Hang up. */
            fprintf(stderr, "Client %d hit errno %d\n", request->client_fd, errno);
            Py_DECREF(request->current_chunk);
            Py_XDECREF(request->iterable);
            request->iterable = NULL; /* to make ev_io_on_write jump right into 'bye' */
            return 0;
        }
    }

    DBG_REQ(request, "Sent %zd bytes from %p", sent_bytes, request->current_chunk);
    request->current_chunk_p += sent_bytes;
    if(request->current_chunk_p == PyString_GET_SIZE(request->current_chunk)) {
        DBG_REQ(request, "Done with string %p", request->current_chunk);
        Py_DECREF(request->current_chunk);
        request->current_chunk = NULL;
        request->current_chunk_p = 0;
        return 0;
    }
    return 1;
}

static inline void
set_error(Request* request, http_status status)
{
    const char* msg;
    switch(status) {
        case HTTP_SERVER_ERROR:
            msg = "HTTP/1.0 500 Internal Server Error\r\n\r\n" \
                  "HTTP 500 Internal Server Error :(";
            break;
        case HTTP_BAD_REQUEST:
            msg = "HTTP/1.0 400 Bad Request\r\n\r\n" \
                  "You sent a malformed request.";
            break;
        case HTTP_LENGTH_REQUIRED:
            msg =  "HTTP/1.0 411 Length Required\r\n";
            break;
        default:
            assert(0);
    }
    request->current_chunk = PyString_FromString(msg);

    if(request->iterable != NULL) {
        Py_DECREF(request->iterable);
        request->iterable = NULL;
    }
}

static inline bool
set_nonblocking(int fd)
{
    int flags = fcntl(fd, F_GETFL, 0);
    return (fcntl(fd, F_SETFL, (flags < 0 ? 0 : flags) | O_NONBLOCK) != -1);
}

#if WANT_SIGINT_HANDLING
static void
ev_signal_on_sigint(struct ev_loop* mainloop, ev_signal* watcher, const int events)
{
    /* Clean up and shut down this thread.
       (Shuts down the Python interpreter if this is the main thread) */
    ev_unloop(mainloop, EVUNLOOP_ALL);
    PyErr_SetInterrupt();
}
#endif
