#include <arpa/inet.h>
#include <fcntl.h>
#include <ev.h>

#if defined(__FreeBSD__) || defined(__DragonFly__)
# include <netinet/in.h> /* for struct sockaddr_in */
# include <sys/types.h>
# include <sys/socket.h>
#endif

#ifdef WANT_SIGINT_HANDLING
# include <sys/signal.h>
#endif

#include "filewrapper.h"
#include "portable_sendfile.h"
#include "common.h"
#include "wsgi.h"
#include "server.h"
#include "py2py3.h"

#ifdef WANT_STATSD
#include "statsd-client.h"
#endif

#define READ_BUFFER_SIZE 64*1024
#define GIL_LOCK(n) PyGILState_STATE _gilstate_##n = PyGILState_Ensure()
#define GIL_UNLOCK(n) PyGILState_Release(_gilstate_##n)

static const char* http_error_messages[5] = {
  NULL, /* Error codes start at 1 because 0 means "no error" */
  "HTTP/1.1 400 Bad Request\r\n\r\n",
  "HTTP/1.1 406 Length Required\r\n\r\n",
  "HTTP/1.1 417 Expectation Failed\r\n\r\n",
  "HTTP/1.1 500 Internal Server Error\r\n\r\n"
};

static const char *CONTINUE = "HTTP/1.1 100 Continue\r\n\r\n";

enum _rw_state {
  not_yet_done = 1,
  done,
  aborted,
  expect_continue,
};
typedef enum _rw_state read_state;
typedef enum _rw_state write_state;

typedef struct {
  ServerInfo* server_info;
  ev_io accept_watcher;
} ThreadInfo;

typedef void ev_io_callback(struct ev_loop*, ev_io*, const int);

#ifdef WANT_SIGINT_HANDLING
typedef void ev_signal_callback(struct ev_loop*, ev_signal*, const int);
static ev_signal_callback ev_signal_on_sigint;
#endif

#ifdef WANT_SIGNAL_HANDLING
typedef void ev_timer_callback(struct ev_loop*, ev_timer*, const int);
static ev_timer_callback ev_timer_ontick;
ev_timer timeout_watcher;
#endif

static ev_io_callback ev_io_on_request;
static ev_io_callback ev_io_on_read;
static ev_io_callback ev_io_on_write;
static write_state on_write_sendfile(struct ev_loop*, Request*);
static write_state on_write_chunk(struct ev_loop*, Request*);
static bool do_send_chunk(Request*);
static bool do_sendfile(Request*);
static bool handle_nonzero_errno(Request*);
static void close_connection(struct ev_loop*, Request*);


void server_run(ServerInfo* server_info)
{
  struct ev_loop* mainloop = ev_loop_new(0);

  ThreadInfo thread_info;
  thread_info.server_info = server_info;

  ev_set_userdata(mainloop, &thread_info);

  ev_io_init(&thread_info.accept_watcher, ev_io_on_request, server_info->sockfd, EV_READ);
  ev_io_start(mainloop, &thread_info.accept_watcher);

#ifdef WANT_SIGINT_HANDLING
  ev_signal sigint_watcher;
  ev_signal_init(&sigint_watcher, ev_signal_on_sigint, SIGINT);
  ev_signal_start(mainloop, &sigint_watcher);
#endif

#ifdef WANT_SIGNAL_HANDLING
  ev_timer_init(&timeout_watcher, ev_timer_ontick, 0., SIGNAL_CHECK_INTERVAL);
  ev_timer_start(mainloop, &timeout_watcher);
  ev_set_priority(&timeout_watcher, EV_MINPRI);
#endif

  /* This is the program main loop */
  Py_BEGIN_ALLOW_THREADS
  ev_run(mainloop, 0);
  ev_loop_destroy(mainloop);
  Py_END_ALLOW_THREADS
}

#ifdef WANT_SIGINT_HANDLING
static void
pyerr_set_interrupt(struct ev_loop* mainloop, struct ev_cleanup* watcher, const int events)
{
  PyErr_SetInterrupt();
  free(watcher);
}

static void
ev_signal_on_sigint(struct ev_loop* mainloop, ev_signal* watcher, const int events)
{
  /* Clean up and shut down this thread.
   * (Shuts down the Python interpreter if this is the main thread) */
  ev_cleanup* cleanup_watcher = malloc(sizeof(ev_cleanup));
  ev_cleanup_init(cleanup_watcher, pyerr_set_interrupt);
  ev_cleanup_start(mainloop, cleanup_watcher);

  ev_io_stop(mainloop, &((ThreadInfo*)ev_userdata(mainloop))->accept_watcher);
  ev_signal_stop(mainloop, watcher);
#ifdef WANT_SIGNAL_HANDLING
  ev_timer_stop(mainloop, &timeout_watcher);
#endif
}
#endif

#ifdef WANT_SIGNAL_HANDLING
static void
ev_timer_ontick(struct ev_loop* mainloop, ev_timer* watcher, const int events)
{
  GIL_LOCK(0);
  PyErr_CheckSignals();
  GIL_UNLOCK(0);
}
#endif

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
    STATSD_INCREMENT("conn.accept.error");
    return;
  }

  int flags = fcntl(client_fd, F_GETFL, 0);
  if(fcntl(client_fd, F_SETFL, (flags < 0 ? 0 : flags) | O_NONBLOCK) == -1) {
    STATSD_INCREMENT("conn.accept.error");
    DBG("Could not set_nonblocking() client %d: errno %d", client_fd, errno);
    return;
  }

  GIL_LOCK(0);

  Request* request = Request_new(
    ((ThreadInfo*)ev_userdata(mainloop))->server_info,
    client_fd,
    inet_ntoa(sockaddr.sin_addr)
  );

  GIL_UNLOCK(0);

  STATSD_INCREMENT("conn.accept.success");

  DBG_REQ(request, "Accepted client %s:%d on fd %d",
          inet_ntoa(sockaddr.sin_addr), ntohs(sockaddr.sin_port), client_fd);

  ev_io_init(&request->ev_watcher, &ev_io_on_read,
             client_fd, EV_READ);
  ev_io_start(mainloop, &request->ev_watcher);
}


static void
start_reading(struct ev_loop *mainloop, Request *request)
{
  ev_io_init(&request->ev_watcher, &ev_io_on_read,
             request->client_fd, EV_READ);
  ev_io_start(mainloop, &request->ev_watcher);
}

static void
start_writing(struct ev_loop *mainloop, Request *request)
{
  ev_io_init(&request->ev_watcher, &ev_io_on_write,
             request->client_fd, EV_WRITE);
  ev_io_start(mainloop, &request->ev_watcher);
}

static void
ev_io_on_read(struct ev_loop* mainloop, ev_io* watcher, const int events)
{
  static char read_buf[READ_BUFFER_SIZE];

  Request* request = REQUEST_FROM_WATCHER(watcher);
  read_state read_state;

  ssize_t read_bytes = read(
    request->client_fd,
    read_buf,
    READ_BUFFER_SIZE
  );

  GIL_LOCK(0);

  if (read_bytes == 0) {
    /* Client disconnected */
    read_state = aborted;
    DBG_REQ(request, "Client disconnected");
    STATSD_INCREMENT("req.error.client_disconnected");
  } else if (read_bytes < 0) {
    /* Would block or error */
    if(errno == EAGAIN || errno == EWOULDBLOCK) {
      read_state = not_yet_done;
    } else {
      read_state = aborted;
      DBG_REQ(request, "Hit errno %d while read()ing", errno);
      STATSD_INCREMENT("req.error.read");
    }
  } else {
    /* OK, either expect more data or done reading */
    Request_parse(request, read_buf, (size_t)read_bytes);
    if(request->state.error_code) {
      /* HTTP parse error */
      read_state = done;
      DBG_REQ(request, "Parse error");
      STATSD_INCREMENT("req.error.parse");
      request->current_chunk = _PEP3333_Bytes_FromString(
        http_error_messages[request->state.error_code]);
      assert(request->iterator == NULL);
    } else if(request->state.parse_finished) {
      /* HTTP parse successful, meaning we have the entire
       * request (the header _and_ the body). */
      read_state = done;

      STATSD_INCREMENT("req.success.read");

      if (!wsgi_call_application(request)) {
        /* Response is "HTTP 500 Internal Server Error" */
        DBG_REQ(request, "WSGI app error");
        assert(PyErr_Occurred());
        PyErr_Print();
        assert(!request->state.chunked_response);
        Py_CLEAR(request->iterator);
        request->current_chunk = _PEP3333_Bytes_FromString(
          http_error_messages[HTTP_SERVER_ERROR]);
        STATSD_INCREMENT("req.error.internal");
      }
    } else if (request->state.expect_continue) {
      /*
      ** Handle "Expect: 100-continue" header.
      ** See https://tools.ietf.org/html/rfc2616#page-48 and `on_header_value`
      ** in request.c for more details.
      */
      read_state = expect_continue;
    } else {
      /* Wait for more data */
      read_state = not_yet_done;
    }
  }

  switch (read_state) {
  case not_yet_done:
    STATSD_INCREMENT("req.active");
    break;
  case expect_continue:
    DBG_REQ(request, "pause read, write 100-continue");
    ev_io_stop(mainloop, &request->ev_watcher);
    request->current_chunk = _PEP3333_Bytes_FromString(CONTINUE);
    start_writing(mainloop, request);
    break;
  case done:
    DBG_REQ(request, "Stop read watcher, start write watcher");
    ev_io_stop(mainloop, &request->ev_watcher);
    start_writing(mainloop, request);
    STATSD_INCREMENT("req.done");
    break;
  case aborted:
    close_connection(mainloop, request);
    STATSD_INCREMENT("req.aborted");
    break;
  }

  GIL_UNLOCK(0);
}

static void
ev_io_on_write(struct ev_loop* mainloop, ev_io* watcher, const int events)
{
  /* Since the response writing code is fairly complex, I'll try to give a short
   * overview of the different control flow paths etc.:
   *
   * On the very top level, there are two types of responses to distinguish:
   * A) sendfile responses
   * B) iterator/other responses
   *
   * These cases are handled by the 'on_write_sendfile' and 'on_write_chunk'
   * routines, respectively.  They use the 'do_sendfile' and 'do_send_chunk'
   * routines to do the actual write()-ing. The 'do_*' routines return true if
   * there's some data left to send in the current chunk (or, in the case of
   * sendfile, the end of the file has not been reached yet).
   *
   * When the 'do_*' routines return false, the 'on_write_*' routines have to
   * figure out if there's a next chunk to send (e.g. in the case of a response iterator).
   */
  Request* request = REQUEST_FROM_WATCHER(watcher);

  GIL_LOCK(0);

  write_state write_state;
  if(request->iterable && FileWrapper_CheckExact(request->iterable) && FileWrapper_GetFd(request->iterable) != -1) {
    write_state = on_write_sendfile(mainloop, request);
  } else {
    write_state = on_write_chunk(mainloop, request);
  }

  write_state = request->state.expect_continue
    ? expect_continue
    : write_state;

  switch(write_state) {
  case not_yet_done:
    STATSD_INCREMENT("resp.active");
    break;
  case done:
    STATSD_INCREMENT("resp.done");
    if(request->state.keep_alive) {
      DBG_REQ(request, "done, keep-alive");
      STATSD_INCREMENT("resp.done.keepalive");
      ev_io_stop(mainloop, &request->ev_watcher);

      Request_clean(request);
      Request_reset(request);

      start_reading(mainloop, request);
    } else {
      DBG_REQ(request, "done, close");
      STATSD_INCREMENT("resp.conn.close");
      close_connection(mainloop, request);
    }
    break;
  case expect_continue:
    DBG_REQ(request, "expect continue");
    ev_io_stop(mainloop, &request->ev_watcher);

    request->state.expect_continue = false;
    start_reading(mainloop, request);
    break;
  case aborted:
    /* Response was aborted due to an error. We can't do anything graceful here
     * because at least one chunk is already sent... just close the connection. */
    close_connection(mainloop, request);
    STATSD_INCREMENT("resp.aborted");
    break;
  }

  GIL_UNLOCK(0);
}

static write_state
on_write_sendfile(struct ev_loop* mainloop, Request* request)
{
  /* A sendfile response is split into two phases:
   * Phase A) sending HTTP headers
   * Phase B) sending the actual file contents
   */
  if(request->current_chunk) {
    /* Phase A) -- current_chunk contains the HTTP headers */
    if (do_send_chunk(request)) {
      /* Headers left to send */
    } else {
      /* Done with headers, current_chunk has been set to NULL
       * and we'll fall into Phase B) on the next invocation. */
      request->current_chunk_p = lseek(FileWrapper_GetFd(request->iterable), 0, SEEK_CUR);
      if (request->current_chunk_p == -1) {
        return aborted;
      }
    }
    return not_yet_done;
  } else {
    /* Phase B) */
    if (do_sendfile(request)) {
      // Haven't reached the end of file yet
      return not_yet_done;
    } else {
      // Done with the file
      return done;
    }
  }
}


static write_state
on_write_chunk(struct ev_loop* mainloop, Request* request)
{
  if (do_send_chunk(request))
    // data left to send in the current chunk
    return not_yet_done;

  if(request->iterator) {
    /* Reached the end of a chunk in the response iterator. Get next chunk. */
    PyObject* next_chunk = wsgi_iterable_get_next_chunk(request);
    if(next_chunk) {
      /* We found another chunk to send. */
      if(request->state.chunked_response) {
        request->current_chunk = wrap_http_chunk_cruft_around(next_chunk);
        Py_DECREF(next_chunk);
      } else {
        request->current_chunk = next_chunk;
      }
      assert(request->current_chunk_p == 0);
      return not_yet_done;

    } else {
      if(PyErr_Occurred()) {
        /* Trying to get the next chunk raised an exception. */
        PyErr_Print();
        DBG_REQ(request, "Exception in iterator, can not recover");
        return aborted;
      } else {
        /* This was the last chunk; cleanup. */
        Py_CLEAR(request->iterator);
        goto send_terminator_chunk;
      }
    }
  } else {
    /* We have no iterator to get more chunks from, so we're done.
     * Reasons we might end up in this place:
     * A) A parse or server error occurred
     * C) We just finished a chunked response with the call to 'do_send_chunk'
     *    above and now maybe have to send the terminating empty chunk.
     * B) We used chunked responses earlier in the response and
     *    are now sending the terminating empty chunk.
     */
    goto send_terminator_chunk;
  }

  assert(0); // unreachable

send_terminator_chunk:
  if(request->state.chunked_response) {
    /* We have to send a terminating empty chunk + \r\n */
    request->current_chunk = _PEP3333_Bytes_FromString("0\r\n\r\n");
    assert(request->current_chunk_p == 0);
    // Next time we get here, don't send the terminating empty chunk again.
    // XXX This is kind of a hack and should be refactored for easier understanding.
    request->state.chunked_response = false;
    return not_yet_done;
  } else {
    return done;
  }
}

/* Return true if there's data left to send, false if we reached the end of the chunk. */
static bool
do_send_chunk(Request* request)
{
  Py_ssize_t bytes_sent;

  assert(request->current_chunk != NULL);
  assert(!(request->current_chunk_p == _PEP3333_Bytes_GET_SIZE(request->current_chunk)
           && _PEP3333_Bytes_GET_SIZE(request->current_chunk) != 0));

  bytes_sent = write(
    request->client_fd,
    _PEP3333_Bytes_AS_DATA(request->current_chunk) + request->current_chunk_p,
    _PEP3333_Bytes_GET_SIZE(request->current_chunk) - request->current_chunk_p
  );

  if(bytes_sent == -1)
    return handle_nonzero_errno(request);

  request->current_chunk_p += bytes_sent;
  if(request->current_chunk_p == _PEP3333_Bytes_GET_SIZE(request->current_chunk)) {
    Py_CLEAR(request->current_chunk);
    request->current_chunk_p = 0;
    return false;
  }
  return true;
}

/* Return true if there's data left to send, false if we reached the end of the file. */
static bool
do_sendfile(Request* request)
{
  Py_ssize_t bytes_sent = portable_sendfile(
      request->client_fd,
      FileWrapper_GetFd(request->iterable),
      request->current_chunk_p
  );
  switch(bytes_sent) {
  case -1:
    if (handle_nonzero_errno(request)) {
      return true;
    } else {
      return false;
    }
  case 0:
    return false;
  default:
    request->current_chunk_p += bytes_sent;
    return true;
  }
}

static bool
handle_nonzero_errno(Request* request)
{
  if(errno == EAGAIN || errno == EWOULDBLOCK) {
    /* Try again later */
    return true;
  } else {
    /* Serious transmission failure. Hang up. */
    fprintf(stderr, "Client %d hit errno %d\n", request->client_fd, errno);
    Py_XDECREF(request->current_chunk);
    Py_CLEAR(request->iterator);
    request->state.keep_alive = false;
    return false;
  }
}

static void
close_connection(struct ev_loop *mainloop, Request* request)
{
  DBG_REQ(request, "Closing socket");
  ev_io_stop(mainloop, &request->ev_watcher);
  close(request->client_fd);
  Request_free(request);
}
