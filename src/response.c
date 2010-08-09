#include "response.h"

void
get_response(Request* request, int parser_exit_code)
{
    DEBUG("Parser exited with %d.", parser_exit_code);
    switch(parser_exit_code) {
        case PARSER_OK:
            /* standard case, call wsgi app */
            wsgi_app(request);
            break;
        case HTTP_INTERNAL_SERVER_ERROR:
            set_http_500_response(request);
            break;
        case HTTP_NOT_FOUND:
            set_http_404_response(request);
            break;
        default:
            assert(0);
    }
}

void
set_http_500_response(Request* request)
{
    request->response_body = PyString_FromString(HTTP_500_MESSAGE);
    request->response_body_position = PyString_AsString(request->response_body);
    request->response_body_length = strlen(HTTP_500_MESSAGE);
    request->headers_sent = true; /* ^ contains the response_body, no additional headers needed */
}

void
set_http_404_response(Request* request)
{
    request->response_body = PyString_FromString(HTTP_404_MESSAGE);
    request->response_body_position = PyString_AsString(request->response_body);
    request->response_body_length = strlen(HTTP_404_MESSAGE);
    request->headers_sent = true; /* ^ contains the response_body, no additional headers needed */
}

void
set_http_400_response(Request* request)
{
    request->response_body = PyString_FromString(HTTP_400_MESSAGE);
    request->response_body_position = PyString_AsString(request->response_body);
    request->response_body_length = strlen(HTTP_400_MESSAGE);
    request->headers_sent = true; /* ^ contains the response_body, no additional headers needed */
}

/*
    Called when the socket is writable, thus, we can send the HTTP response.
*/
void
while_sock_canwrite(EV_LOOP* mainloop, ev_io* write_watcher_, int revents)
{
    DEBUG("Can write.");
    Request* request = OFFSETOF(write_watcher, write_watcher_, Request);

    switch(wsgi_send_response(request)) {
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
    ev_io_stop(mainloop, &request->write_watcher);
    wsgi_finalize(request);
    close(request->client_fd);
    Request_free(request);
}

static int
wsgi_send_response(Request* request)
{
    /* TODO: Maybe it's faster to write HTTP headers and response body at once? */
    if(!request->headers_sent) {
        /* First time we write to this client, send HTTP headers. */
        wsgi_send_headers(request);
        request->headers_sent = true;
        return RESPONSE_NOT_YET_FINISHED;
    } else {
        if(request->use_sendfile)
            return wsgi_sendfile(request);
        else
            return wsgi_send_body(request);
    }
}

static int
wsgi_send_body(Request* request)
{
    ssize_t bytes_sent = write(
        request->client_fd,
        request->response_body_position,
        request->response_body_length
    );
    if(bytes_sent == -1) {
        /* socket error [note 1] */
        if(errno == EAGAIN)
            return RESPONSE_NOT_YET_FINISHED; /* Try again next time. */
        else
            return RESPONSE_SOCKET_ERROR_OCCURRED;
    }
    else {
        request->response_body_length -= bytes_sent;
        request->response_body_position += bytes_sent;
    }

    if(request->response_body_length == 0)
        return RESPONSE_FINISHED;
    else
        return RESPONSE_NOT_YET_FINISHED;
}

static void
wsgi_send_headers(Request* request)
{
    char        buf[MAX_HEADER_SIZE]; memcpy((char*)buf, "HTTP/1.1 ", 9);
    size_t      bufpos = 9;
    bool        have_content_length = false;

    #define BUFPOS (((char*)buf)+bufpos)
    #define COPY_STRING(obj) do{{ \
        c_size_t s_len = PyString_GET_SIZE(obj); \
        memcpy(BUFPOS, PyString_AS_STRING(obj), s_len); \
        bufpos += s_len; \
    }}while(0)
    #define NEWLINE do{ buf[bufpos++] = '\r'; \
                        buf[bufpos++] = '\n'; } while(0)

    /* Copy the HTTP status message into the buffer: */
    COPY_STRING(request->status);
    NEWLINE;

    size_t number_of_headers = PyList_GET_SIZE(request->headers);
    for(unsigned int i=0; i<number_of_headers; ++i)
    {
        #define item PyList_GET_ITEM(request->headers, i)
        COPY_STRING(PyTuple_GET_ITEM(item, 0));
        buf[bufpos++] = ':';
        buf[bufpos++] = ' ';
        COPY_STRING(PyTuple_GET_ITEM(item, 1));
        NEWLINE;
        #undef item
    }

    /* Make sure a Content-Length header is set: */
    if(!have_content_length) {
        memcpy(BUFPOS, "Content-Length: ", 16);
        bufpos += 16;
        DEBUG("Body length: %d", request->response_body_length);
        bufpos += uitoa10(request->response_body_length, BUFPOS);
        NEWLINE;
    }

    NEWLINE;

    #undef NEWLINE
    #undef COPY_STRING
    #undef BUFPOS

    write(request->client_fd, buf, bufpos);

    Py_DECREF(request->headers);
    Py_DECREF(request->status);
}

static void
wsgi_finalize(Request* request)
{
    GIL_LOCK();

    PyObject* close_method = PyObject_GetAttr(request->response_body, _(close));
    if(!close_method)
        PyErr_Clear(); /* ignore the absence of a .close method */
    else
        if(!PyObject_CallObject(close_method, NULL))
            PyErr_Print(); /* Exception in .close() */

    GIL_UNLOCK();
}
