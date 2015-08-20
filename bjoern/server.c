#include <arpa/inet.h>
#include <fcntl.h>
#include <ev.h>

#if defined __FreeBSD__
# include <netinet/in.h> /* for struct sockaddr_in */
# include <sys/types.h>
# include <sys/socket.h>
#endif

#ifdef WANT_SIGINT_HANDLING
# include <sys/signal.h>
#endif

#include "portable_sendfile.h"
#include "common.h"
#include "wsgi.h"
#include "server.h"

#define SERVER_INFO(loop) ((ServerInfo*)ev_userdata(loop))
#define READ_BUFFER_SIZE 64*1024
#define Py_XCLEAR(obj) do { if(obj) { Py_DECREF(obj); obj = NULL; } } while(0)
#define GIL_LOCK(n) PyGILState_STATE _gilstate_##n = PyGILState_Ensure()
#define GIL_UNLOCK(n) PyGILState_Release(_gilstate_##n)

static const char* http_error_messages[4] = {
  NULL, /* Error codes start at 1 because 0 means "no error" */
  "HTTP/1.1 400 Bad Request\r\n\r\n",
  "HTTP/1.1 406 Length Required\r\n\r\n",
  "HTTP/1.1 500 Internal Server Error\r\n\r\n"
};

enum write_state {
  not_yet_done = 1,
  done,
  aborted,
};

typedef void ev_io_callback(struct ev_loop*, ev_io*, const int);

#if WANT_SIGINT_HANDLING
typedef void ev_signal_callback(struct ev_loop*, ev_signal*, const int);
static ev_signal_callback ev_signal_on_sigint;
#endif

static ev_io_callback ev_io_on_request;
static ev_io_callback ev_io_on_read;
static ev_io_callback ev_io_on_write;
static enum write_state on_write_sendfile(struct ev_loop*, Request*);
static enum write_state on_write_chunk(struct ev_loop*, Request*);
static bool do_send_chunk(Request*);
static bool do_sendfile(Request*);
static bool handle_nonzero_errno(Request*);


void server_run(ServerInfo* server_info)
{
  struct ev_loop* mainloop = ev_loop_new(0);
  ev_set_userdata(mainloop, server_info);

  ev_io accept_watcher;
  ev_io_init(&accept_watcher, ev_io_on_request, server_info->sockfd, EV_READ);
  ev_io_start(mainloop, &accept_watcher);

#if WANT_SIGINT_HANDLING
  ev_signal signal_watcher;
  ev_signal_init(&signal_watcher, ev_signal_on_sigint, SIGINT);
  ev_signal_start(mainloop, &signal_watcher);
#endif

  /* This is the program main loop */
  Py_BEGIN_ALLOW_THREADS
  ev_loop(mainloop, 0);
  ev_default_destroy();
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

  Request* request = Request_new(
    SERVER_INFO(mainloop),
    client_fd,
    inet_ntoa(sockaddr.sin_addr)
  );

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

  ssize_t read_bytes = read(
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
      ev_io_stop(mainloop, &request->ev_watcher);
      Request_free(request);
    }
    goto out;
  }

  Request_parse(request, read_buf, (size_t)read_bytes);

  if(request->state.error_code) {
    DBG_REQ(request, "Parse error");
    request->current_chunk = PyString_FromString(
      http_error_messages[request->state.error_code]);
    assert(request->iterator == NULL);
  }
  else if(request->state.parse_finished) {
    if(!wsgi_call_application(request)) {
      assert(PyErr_Occurred());
      PyErr_Print();
      assert(!request->state.chunked_response);
      Py_XCLEAR(request->iterator);
      request->current_chunk = PyString_FromString(
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

  enum write_state write_state;
  if(request->state.use_sendfile) {
    write_state = on_write_sendfile(mainloop, request);
  } else {
    write_state = on_write_chunk(mainloop, request);
  }

  switch(write_state) {
  case not_yet_done:
    break;
  case done:
    /* Done with the response. */
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
    break;
  case aborted:
    /* Response was aborted due to an error. We can't do anything graceful here
     * because at least one chunk is already sent... just close the connection. */
    ev_io_stop(mainloop, &request->ev_watcher);
    close(request->client_fd);
    Request_free(request);
    break;
  }

  GIL_UNLOCK(0);
}

static enum write_state
on_write_sendfile(struct ev_loop* mainloop, Request* request)
{
  /* A sendfile response is split into two phases:
   * Phase A) sending HTTP headers
   * Phase B) sending the actual file contents
   */
  if(request->current_chunk) {
    /* Phase A) -- current_chunk contains the HTTP headers */
    if (do_send_chunk(request)) {
      // data left to send in the current chunk
      return not_yet_done;
    } else {
      assert(request->current_chunk == NULL);
      assert(request->current_chunk_p == 0);
      /* Transition to Phase B) -- abuse current_chunk_p to store the file fd */
      request->current_chunk_p = PyObject_AsFileDescriptor(request->iterable);
      // don't stop yet, Phase B is still missing
      return not_yet_done;
    }
  } else {
    /* Phase B) -- current_chunk_p contains file fd */
    if (do_sendfile(request)) {
      // Haven't reached the end of file yet
      return not_yet_done;
    } else {
      // Done with the file
      return done;
    }
  }
}


static enum write_state
on_write_chunk(struct ev_loop* mainloop, Request* request)
{
  if (do_send_chunk(request))
    // data left to send in the current chunk
    return not_yet_done;

  if(request->iterator) {
    /* Reached the end of a chunk in the response iterator. Get next chunk. */
    PyObject* next_chunk;
    next_chunk = wsgi_iterable_get_next_chunk(request);
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
    request->current_chunk = PyString_FromString("0\r\n\r\n");
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
  assert(!(request->current_chunk_p == PyString_GET_SIZE(request->current_chunk)
           && PyString_GET_SIZE(request->current_chunk) != 0));

  bytes_sent = write(
    request->client_fd,
    PyString_AS_STRING(request->current_chunk) + request->current_chunk_p,
    PyString_GET_SIZE(request->current_chunk) - request->current_chunk_p
  );

  if(bytes_sent == -1)
    return handle_nonzero_errno(request);

  request->current_chunk_p += bytes_sent;
  if(request->current_chunk_p == PyString_GET_SIZE(request->current_chunk)) {
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
      request->current_chunk_p /* current_chunk_p stores the file fd */
  );
  if(bytes_sent == -1)
    return handle_nonzero_errno(request);
  return bytes_sent != 0;
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
    Py_XCLEAR(request->iterator);
    request->state.keep_alive = false;
    return false;
  }
}
