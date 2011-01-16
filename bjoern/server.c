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
#include "py2to3.h"

#define LISTEN_BACKLOG  1024
#define READ_BUFFER_SIZE 64*1024
#define Py_XCLEAR(obj) do { if(obj) { Py_DECREF(obj); obj = NULL; } } while(0)
#define GIL_LOCK(n) PyGILState_STATE _gilstate_##n = PyGILState_Ensure()
#define GIL_UNLOCK(n) PyGILState_Release(_gilstate_##n)

static int sockfd;
static const char* http_error_messages[4] = {
  NULL, /* Error codes start at 1 because 0 means "no error" */
  "HTTP/1.1 400 Bad Request\r\n\r\n",
  "HTTP/1.1 406 Length Required\r\n\r\n",
  "HTTP/1.1 500 Internal Server Error\r\n\r\n"
};

typedef void ev_io_callback(struct ev_loop*, ev_io*, const int);
#if WANT_SIGINT_HANDLING
typedef void ev_signal_callback(struct ev_loop*, ev_signal*, const int);
static ev_signal_callback ev_signal_on_sigint;
#endif
static ev_io_callback ev_io_on_request;
static ev_io_callback ev_io_on_read;
static ev_io_callback ev_io_on_write;
static bool send_chunk(Request*);

void server_run(const char* hostaddr, const int port)
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

#if WANT_SIGINT_HANDLING
static void
ev_signal_on_sigint(struct ev_loop* mainloop, ev_signal* watcher, const int events)
{
  /* Clean up and shut down this thread.
   * (Shuts down the Python interpreter if this is the main thread) */
  ev_unloop(mainloop, EVUNLOOP_ALL);
  PyErr_SetInterrupt();
}
#endif

bool server_init(const char* hostaddr, const int port)
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
  struct sockaddr_in sockaddr;
  socklen_t addrlen;

  addrlen = sizeof(struct sockaddr_in);
  client_fd = accept(watcher->fd, (struct sockaddr*)&sockaddr, &addrlen);
  if(client_fd < 0) {
    DBG("Could not accept() client: errno %d", errno);
    return;
  }

  int flags = fcntl(client_fd, F_GETFL, 0);
  if(fcntl(client_fd, F_SETFL, (flags < 0 ? 0 : flags) | O_NONBLOCK) == -1) {
    DBG("Could not set_nonblocking() client %d: errno %d", client_fd, errno);
    return;
  }

  GIL_LOCK(0);
  Request* request = Request_new(client_fd, inet_ntoa(sockaddr.sin_addr));
  GIL_UNLOCK(0);

  DBG_REQ(request, "Accepted client %s:%d on fd %d",
          inet_ntoa(sockaddr.sin_addr), ntohs(sockaddr.sin_port), client_fd);

  ev_io_init(&request->ev_watcher, &ev_io_on_read,
             client_fd, EV_READ);
  ev_io_start(mainloop, &request->ev_watcher);
}

static void
ev_io_on_read(struct ev_loop* mainloop, ev_io* watcher, const int events)
{
  static char read_buf[READ_BUFFER_SIZE];

  Request* request = REQUEST_FROM_WATCHER(watcher);

  Py_ssize_t read_bytes = read(
    request->client_fd,
    read_buf,
    READ_BUFFER_SIZE
  );

  GIL_LOCK(0);

  if(read_bytes <= 0) {
    if(errno != EAGAIN && errno != EWOULDBLOCK) {
      if(read_bytes == 0)
        DBG_REQ(request, "Client disconnected");
      else
        DBG_REQ(request, "Hit errno %d while read()ing", errno);
      close(request->client_fd);
      Request_free(request);
      ev_io_stop(mainloop, &request->ev_watcher);
    }
    goto out;
  }

  Request_parse(request, read_buf, read_bytes);

  if(request->state.error_code) {
    DBG_REQ(request, "Parse error");
    request->current_chunk = PyBytes_FromString(  /* Unicode sends garbage */
      http_error_messages[request->state.error_code]);  /* msg to telnet ? */
    assert(request->iterable == NULL);
  }
  else if(request->state.parse_finished) {
    if(!wsgi_call_application(request)) {
      assert(PyErr_Occurred());
      PyErr_Print();
      assert(!request->state.chunked_response);
      Py_XCLEAR(request->iterable);
      request->current_chunk = PyBytes_FromString(  /* same as above */
        http_error_messages[HTTP_SERVER_ERROR]);
    }
  } else {
    /* Wait for more data */
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
  Request* request = REQUEST_FROM_WATCHER(watcher);

  GIL_LOCK(0);
  assert(request->current_chunk);

  if(send_chunk(request))
    goto out;

  if(request->iterable) {
    PyObject* next_chunk;
    next_chunk = wsgi_iterable_get_next_chunk(request);
    if(next_chunk) {
      if(request->state.chunked_response) {
        request->current_chunk = wrap_http_chunk_cruft_around(next_chunk);
        Py_DECREF(next_chunk);
      } else {
        request->current_chunk = next_chunk;
      }
      assert(request->current_chunk_p == 0);
      goto out;
    } else {
      if(PyErr_Occurred()) {
        PyErr_Print();
        /* We can't do anything graceful here because at least one
         * chunk is already sent... just close the connection */
        DBG_REQ(request, "Exception in iterator, can not recover");
        ev_io_stop(mainloop, &request->ev_watcher);
        close(request->client_fd);
        Request_free(request);
        goto out;
      }
      Py_DECREF(request->iterable);
      request->iterable = NULL;
    }
  }

  if(request->state.chunked_response) {
    /* We have to send a terminating empty chunk + \r\n */
    request->current_chunk = PyBytes_FromString("0\r\n\r\n");
    assert(request->current_chunk_p == 0);
    request->state.chunked_response = false;
    goto out;
  }

  ev_io_stop(mainloop, &request->ev_watcher);
  if(request->state.keep_alive) {
    DBG_REQ(request, "done, keep-alive");
    Request_clean(request);
    Request_reset(request);
    ev_io_init(&request->ev_watcher, &ev_io_on_read,
               request->client_fd, EV_READ);
    ev_io_start(mainloop, &request->ev_watcher);
  } else {
    DBG_REQ(request, "done, close");
    close(request->client_fd);
    Request_free(request);
  }

out:
  GIL_UNLOCK(0);
}

static bool
send_chunk(Request* request)
{
  Py_ssize_t chunk_length;
  Py_ssize_t sent_bytes;

  assert(request->current_chunk != NULL);
  assert(!(request->current_chunk_p == PyBytes_GET_SIZE(request->current_chunk)
         && PyBytes_GET_SIZE(request->current_chunk) != 0));

  sent_bytes = write(
    request->client_fd,
    PyBytes_AS_STRING(request->current_chunk) + request->current_chunk_p,
    PyBytes_GET_SIZE(request->current_chunk) - request->current_chunk_p
  );

  if(sent_bytes == -1) {
    error:
    if(errno == EAGAIN || errno == EWOULDBLOCK) {
      /* Try again later */
      return true;
    } else {
      /* Serious transmission failure. Hang up. */
      fprintf(stderr, "Client %d hit errno %d\n", request->client_fd, errno);
      Py_DECREF(request->current_chunk);
      Py_XCLEAR(request->iterable);
      request->state.keep_alive = false;
      return false;
    }
  }

  request->current_chunk_p += sent_bytes;
  if(request->current_chunk_p == PyBytes_GET_SIZE(request->current_chunk)) {
    Py_DECREF(request->current_chunk);
    request->current_chunk = NULL;
    request->current_chunk_p = 0;
    return false;
  }
  return true;
}
