#include "request.h"

Request* Request_new()
{
    Request* request = calloc(1, sizeof(Request));
    request->parser = Parser_new();
    request->parser->request = request;
    return request;
}

void Request_free(Request* request)
{
    GIL_LOCK();
    Py_DECREF(request->response_body);
    Py_XDECREF(request->request_environ);
#ifdef WANT_ROUTING
    Py_XDECREF(request->route_kwargs);
#endif
    GIL_UNLOCK();
    free(request->parser);
    free(request);
}

/*
    Called when a new client is accepted on the socket file.
*/
void
on_sock_accept(EV_LOOP* mainloop, ev_io* accept_watcher, int revents)
{
    Request* request = Request_new();

    /* Accept the socket connection. */
    struct sockaddr_in client_address;
    socklen_t client_address_length = sizeof(client_address);
    request->client_fd = accept(
        accept_watcher->fd,
        (struct sockaddr *)&client_address,
        &client_address_length
    );

    if(request->client_fd < 0) return; /* ERROR!1!1! */

    DEBUG("Accepted client on %d.", request->client_fd);

    /* Set the file descriptor non-blocking. */
    int v = fcntl(request->client_fd, F_GETFL, 0);
    fcntl(request->client_fd, F_SETFL, (v < 0 ? 0 : v) | O_NONBLOCK);

    /* Run the read-watch loop. */
    ev_io_init(&request->read_watcher, &on_sock_read, request->client_fd, EV_READ);
    ev_io_start(mainloop, &request->read_watcher);
}


/*
    Called when the socket is readable, thus, an HTTP request waits to be parsed.
*/
void
on_sock_read(EV_LOOP* mainloop, ev_io* read_watcher_, int revents)
{
    /* Check whether we can still read. */
    if(!(revents & EV_READ))
        return;

    char    read_buffer[READ_BUFFER_SIZE];
    ssize_t bytes_read;
    size_t  bytes_parsed;

    Request* request = OFFSETOF(read_watcher, read_watcher_, Request);

    /* Read from the client: */
    bytes_read = read(request->client_fd, read_buffer, READ_BUFFER_SIZE);
    DEBUG("Read %d bytes.", bytes_read);

    switch(bytes_read) {
        case  0: goto read_finished;
        case -1: return; /* An error happened. Try again next time libev calls
                            this function. TODO: Limit retries? */
        default:
            request->received_anything = true;
            size_t bytes_parsed = Parser_execute(
                request->parser,
                read_buffer,
                bytes_read
            );
            DEBUG("Parsed %d/%d bytes", bytes_parsed, bytes_read);
            if(bytes_parsed != bytes_read)
                set_http_400_response(request);
            else
                get_response(
                    request,
                    /* exit code */ (int)((http_parser*)request->parser)->data
                );
    }

read_finished:
    if(!request->received_anything) {
        DEBUG("HTTP 400 Bad Request for that beautiful "
              "0 byte request on socket %d.", request->client_fd);
        set_http_400_response(request);
    }
    DEBUG("Read finished!");
    /* Stop the read loop, we're done reading. */
    ev_io_stop(mainloop, &request->read_watcher);
    goto start_write;

start_write:
    ev_io_init(&request->write_watcher, &while_sock_canwrite,
               request->client_fd, EV_WRITE);
    ev_io_start(mainloop, &request->write_watcher);
}
