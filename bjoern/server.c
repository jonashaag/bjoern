#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#ifdef WANT_SIGINT_HANDLING
# include <sys/signal.h>
#endif
#ifdef TLS_SUPPORT
# include <openssl/x509.h>
# include <openssl/ssl.h>
# include <openssl/err.h>
#endif
#include <sys/sendfile.h>
#include <ev.h>
#include "common.h"
#include "wsgi.h"
#include "server.h"

#define LISTEN_BACKLOG  1024
#define READ_BUFFER_SIZE 64*1024
#define SENDFILE_CHUNK_SIZE 16*1024
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

// ssl context SSL_CTX *
static void* ctx = NULL;

typedef void ev_io_callback(struct ev_loop*, ev_io*, const int);
#if WANT_SIGINT_HANDLING
typedef void ev_signal_callback(struct ev_loop*, ev_signal*, const int);
static ev_signal_callback ev_signal_on_sigint;
#endif
static ev_io_callback ev_io_on_request;
static ev_io_callback ev_io_on_read;
static ev_io_callback ev_io_on_write;

static int read_socket(Request*, char*);
static int write_socket(Request*, char*, int);
static int sendfile_socket(Request*, int);
static void close_socket(Request*);


//used by tls
static ev_io_callback ev_io_on_tls_request;
static ev_io_callback ev_io_on_handshake;

static int read_tls(Request*, char*);
static int write_tls(Request*, char*, int);
static int sendfile_tls(Request*, int);
static void close_tls(Request*);


static Request* accept_request(ev_io* watcher);
static bool send_chunk(Request*);
static bool do_sendfile(Request*);
static bool handle_nonzero_errno(Request*);

#if TLS_SUPPORT

bool create_tls_context(char* key, char* cert, char* cipher){
  DBG("Creating TLS Context.");
  
  SSL_library_init();
  SSL_load_error_strings();
  SSL_CTX * context = SSL_CTX_new(TLSv1_server_method());
    
  if (SSL_CTX_use_certificate_file(context, cert, SSL_FILETYPE_PEM) <= 0){
    ERR_print_errors_fp(stderr);
    return false;    
  }

  if (SSL_CTX_use_RSAPrivateKey_file(context, key, SSL_FILETYPE_PEM) <= 0){
    ERR_print_errors_fp(stderr);
    return false;
  }
  
  if (!SSL_CTX_check_private_key(context)){
    DBG("Private key dont matches certificate.");
    return false;
  }
  
  if(cipher)
    SSL_CTX_set_cipher_list(context, cipher);
     
  ctx = (void*) context;
  
  return true;
}
#endif

void server_run(const char* hostaddr, const int port)
{
  struct ev_loop* mainloop = ev_default_loop(0);

  ev_io accept_watcher;
  if(!ctx)
    ev_io_init(&accept_watcher, ev_io_on_request, sockfd, EV_READ);
  else
    ev_io_init(&accept_watcher, ev_io_on_tls_request, sockfd, EV_READ);
  
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
  memset(&sockaddr, 0, sizeof sockaddr);
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

static Request* accept_request(ev_io* watcher){
  int client_fd;
  struct sockaddr_in sockaddr;
  socklen_t addrlen;

  addrlen = sizeof(struct sockaddr_in);
  client_fd = accept(watcher->fd, (struct sockaddr*)&sockaddr, &addrlen);
  if(client_fd < 0) {
    DBG("Could not accept() client: errno %d", errno);
    return NULL;
  }

  int flags = fcntl(client_fd, F_GETFL, 0);
  if(fcntl(client_fd, F_SETFL, (flags < 0 ? 0 : flags) | O_NONBLOCK) == -1) {
    DBG("Could not set_nonblocking() client %d: errno %d", client_fd, errno);
    return NULL;
  }

  GIL_LOCK(0);
  Request* request = Request_new(client_fd, inet_ntoa(sockaddr.sin_addr));
  GIL_UNLOCK(0);

  DBG_REQ(request, "Accepted client %s:%d on fd %d",
          inet_ntoa(sockaddr.sin_addr), ntohs(sockaddr.sin_port), client_fd);
  
  return request;
}

static void
ev_io_on_request(struct ev_loop* mainloop, ev_io* watcher, const int events)
{

  Request* request = accept_request(watcher);
  if(!request) return;
  
  request->read = &read_socket;
  request->write = &write_socket;
  request->sendfile = &sendfile_socket;
  request->close = &close_socket;

  ev_io_init(&request->ev_watcher, &ev_io_on_read,
             request->client_fd, EV_READ);
  ev_io_start(mainloop, &request->ev_watcher);
}

#ifdef TLS_SUPPORT
static void
ev_io_on_tls_request(struct ev_loop* mainloop, ev_io* watcher, const int events)
{

  Request* request = accept_request(watcher);
  if(!request) return;
  
  request->read = &read_tls;
  request->write = &write_tls;
  request->sendfile = &sendfile_tls;
  request->close = &close_tls;
  
  SSL* ssl = SSL_new((SSL_CTX*) ctx);
  SSL_set_mode(ssl, SSL_MODE_ENABLE_PARTIAL_WRITE);
  SSL_set_mode(ssl, SSL_MODE_RELEASE_BUFFERS);
  SSL_set_accept_state(ssl);
  SSL_set_fd(ssl, request->client_fd);
  
  request->ssl = ssl;
  
  DBG("SSL cipher: %s", SSL_get_cipher_list(ssl, 0));

  ev_io_init(&request->ev_watcher, &ev_io_on_handshake,
             request->client_fd, EV_READ);
  ev_io_start(mainloop, &request->ev_watcher);
}

static void 
ev_io_on_handshake(struct ev_loop* mainloop, ev_io* watcher, const int events)
{
  Request* request = REQUEST_FROM_WATCHER(watcher);
  int t = SSL_do_handshake(request->ssl);
  
  if(t == 1){
      ev_io_stop(mainloop, &request->ev_watcher);
      ev_io_init(&request->ev_watcher, &ev_io_on_read,
             watcher->fd, EV_READ);
      ev_io_start(mainloop, &request->ev_watcher);
  }else {
    int err = SSL_get_error(request->ssl, t);
    if (err == SSL_ERROR_WANT_READ) {
      // wait for more data
    }
    else {
      if(err == SSL_ERROR_SSL || err == SSL_ERROR_SYSCALL){
	unsigned long ec = ERR_get_error();
	DBG_REQ(request, "SSL Error %s" ,ERR_error_string(ec, NULL));
      }
      DBG_REQ(request, "Unexpected SSL error, closed request: %d", err);
      close(request->client_fd);
      SSL_free(request->ssl);
      Request_free(request);
      ev_io_stop(mainloop, &request->ev_watcher);
    }
      
  }
}

static int
read_tls(Request* request, char* buf){
  return SSL_read(
     request->ssl, buf, READ_BUFFER_SIZE
  );
}

static int
write_tls(Request* request, char* buf, int size){
  return SSL_write(
     request->ssl, buf, size
   );
}

static int
sendfile_tls(Request* request, int fp){
  char buf[SENDFILE_CHUNK_SIZE];
  int read_bytes;
    
  read_bytes = read(fp, buf, SENDFILE_CHUNK_SIZE);

  if (read_bytes == 0)
        return 0;
   else if (read_bytes < 0)
        return -1;

   SSL_write(request->ssl, buf, read_bytes);
   
   return read_bytes;

}

static void
close_tls(Request* request){
    SSL_shutdown(request->ssl);
    close(request->client_fd);
    SSL_free(request->ssl);
}
#else

static void
ev_io_on_tls_request(struct ev_loop* mainloop, ev_io* watcher, const int events)
{
    // compiler requires this definition
    printf("Should not have been used. Report Error.\n");
}

#endif

static void
ev_io_on_read(struct ev_loop* mainloop, ev_io* watcher, const int events)
{
  static char read_buf[READ_BUFFER_SIZE];

  Request* request = REQUEST_FROM_WATCHER(watcher);

  Py_ssize_t read_bytes = (*request->read)(request, read_buf);
  
  GIL_LOCK(0);

  if(read_bytes <= 0) {
    if(errno != EAGAIN && errno != EWOULDBLOCK) {
      if(read_bytes == 0)
        DBG_REQ(request, "Client disconnected");
      else
        DBG_REQ(request, "Hit errno %d while read()ing", errno);
      (*request->close)(request);
      Request_free(request);
      ev_io_stop(mainloop, &request->ev_watcher);
    }
    goto out;
  }

  Request_parse(request, read_buf, read_bytes);

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
  return;
}

static void
ev_io_on_write(struct ev_loop* mainloop, ev_io* watcher, const int events)
{
  Request* request = REQUEST_FROM_WATCHER(watcher);

  GIL_LOCK(0);

  if(request->state.use_sendfile) {
    /* sendfile */
    if(request->current_chunk && send_chunk(request))
      goto out;
    /* abuse current_chunk_p to store the file fd */
    request->current_chunk_p = PyObject_AsFileDescriptor(request->iterable);
    if(do_sendfile(request))
      goto out;
  } else {
    /* iterable */
    if(send_chunk(request))
      goto out;

    if(request->iterator) {
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
          (*request->close)(request);
          Request_free(request);
          goto out;
        }
        Py_CLEAR(request->iterator);
      }
    }

    if(request->state.chunked_response) {
      /* We have to send a terminating empty chunk + \r\n */
      request->current_chunk = PyString_FromString("0\r\n\r\n");
      assert(request->current_chunk_p == 0);
      request->state.chunked_response = false;
      goto out;
    }
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
    (*request->close)(request);
    Request_free(request);
  }

out:
  GIL_UNLOCK(0);
}

static int
read_socket(Request* request, char* buf){
  return read(
     request->client_fd, buf, READ_BUFFER_SIZE
  );
}

static int
write_socket(Request* request, char* buf, int size){
  return write(
     request->client_fd, buf, size
   );
}

static int
sendfile_socket(Request* request, int fd){
  return sendfile(
    request->client_fd, fd,
    NULL, SENDFILE_CHUNK_SIZE
  );
}

static void
close_socket(Request* request){
    close(request->client_fd);
}


static bool
send_chunk(Request* request)
{
  Py_ssize_t chunk_length;
  Py_ssize_t bytes_sent;

  assert(request->current_chunk != NULL);
  assert(!(request->current_chunk_p == PyString_GET_SIZE(request->current_chunk)
         && PyString_GET_SIZE(request->current_chunk) != 0));

  bytes_sent = (*request->write)(
    request, 
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

static bool
do_sendfile(Request* request)
{
  DBG_REQ(request, "Send file %d", request->current_chunk_p);
  Py_ssize_t bytes_sent = (*request->sendfile)(request, request->current_chunk_p);
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
