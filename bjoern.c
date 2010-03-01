#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stddef.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <ev.h>

#define EV_LOOP struct ev_loop
#define GET_CLIENT_FROM_EV_IO(EV_IO_S, EV_IO_F) (struct Client*) (((char*)EV_IO_S) - offsetof(struct Client, EV_IO_F))
#define HTTP_REQUEST_BUFFER_LENGTH  1024
#define HTTP_RESPONSE_BUFFER_LENGTH 1024

#define DEBUG_ON
#ifdef DEBUG_ON
#define DEBUG(X) printf("DEBUG: " X "\n")
#define DEBUG2(X, Y) printf("DEBUG: " X "\n", Y)
#else
#define DEBUG(x)
#endif
#define ERROR(X) printf("ERROR: " X "\n")


typedef enum {
    false = 0,
    true = 1
} bool;

struct Client {
    ev_io ev_write;
    ev_io ev_read;
    int fd;

    char* header;
    char* body;
    size_t input_position;
};



int
set_nonblock(int fd) {
    int flags;

    flags = fcntl(fd, F_GETFL);
    if (flags < 0)
            return flags;
    flags |= O_NONBLOCK;
    if (fcntl(fd, F_SETFL, flags) < 0)
            return -1;

    return 0;
}


static inline int
get_client_fd(struct ev_io* input_stream) {
    return 1;
    struct sockaddr_in client_address;
    socklen_t client_address_length = sizeof(client_address);

    return accept(
        input_stream->fd,
        (struct sockaddr*)&client_address,
        &client_address_length
    );
}

static const char*
find_http_header_end(const char* c, size_t l) {
    /* modified version of http://ptrace.fefe.de/boyermoore.c.txt */
    size_t i;
    for (i=0; i+1<l; i+=2) {
        if (c[i+1]=='\n') {
            if (c[i]=='\n') return c+i;
            else if (c[i]=='\r' && i+3<l && c[i+2]=='\r' && c[i+3]=='\n')
                return c+i;
            --i;
        } else if (c[i+1]=='\r') {
            if (i+4<l && c[i+2]=='\n' && c[i+3]=='\r' && c[i+4]=='\n')
                return c+i+1;
            --i;
        }
    }
    return 0;
}



static void
close_connection(struct Client *client)
{
    free(client->header);
    free(client->body);
    close(client->fd);
    free(client);
}


static bool
client_write(struct Client* client, const char* output, int output_length) {
    size_t sent_output;
    while(output_length) {
        sent_output = write(client->fd, output, HTTP_RESPONSE_BUFFER_LENGTH);
        if(sent_output < 0) {
            return false;
        }
        else if(sent_output == 0) {
            return true;
        }
        else {
            output += sent_output;
            output_length -= sent_output;
        }
    }
    return true;
}

static void
start_write(struct ev_loop* loop, struct ev_io* input_stream, const int revents) {
    struct Client* client = GET_CLIENT_FROM_EV_IO(input_stream, ev_write);
    if(!(revents & EV_WRITE))
        return;
    client_write(client, "HTTP/1.1 200 Alles OK\r\n\r\nIt works!", 40);
    ev_io_stop(loop, input_stream);
    close_connection(client);
}


static void
on_client_connected(struct ev_loop* loop, struct ev_io* input_stream, const int revents) {
    if(!(revents & EV_READ))
        return; // something went terribly wrong.

    struct Client* client = GET_CLIENT_FROM_EV_IO(input_stream, ev_read);
    size_t input_length;
    char input_buffer[HTTP_REQUEST_BUFFER_LENGTH];
    bool read_finished;

    input_length = read(client->fd, &input_buffer, HTTP_REQUEST_BUFFER_LENGTH);
    if(input_length < 0) {
        ev_io_stop(loop, input_stream);
        close_connection(client);
        return;
    }
    else if(input_length == 0) {
        read_finished = true;
    }
    else {
        client->header = realloc(client->header, (client->input_position + input_length + 1)*sizeof(char));
        memcpy(client->header + client->input_position, &input_buffer, input_length);
        client->input_position += input_length;
        client->header[client->input_position] = '\0';

        // find the position where the request body starts
        char* header_end = (char*)find_http_header_end(client->header, strlen(client->header));
        DEBUG2("HEADER END %d", (int)header_end);
        if(!header_end)
            goto get_out_of_here;
        int header_length = header_end - client->header;
        client->body = header_end; // TODO: +2 or +4
        DEBUG2("CHAR %c", *header_end-1);
        *header_end = '\0'; // end the header. TODO: This makes at most 3, at least 1 byte(s) unfreeable

        char* content_length = strstr(client->header, "Content-Length: ");
        if(!content_length) {
            read_finished = true;
        }
        else {
            int body_length = strtol(content_length+16, NULL, 10); // TODO: Maybe we don't need `strtol` here
            if(client->input_position >= body_length + header_length)
                read_finished = true;
        }
    }

get_out_of_here:

    if(read_finished) {
        ev_io_stop(loop, input_stream);
        if(strlen(client->header)) {
            ev_io_init(&client->ev_write, &start_write, client->fd, EV_WRITE);
            ev_io_start(loop, &client->ev_write);
        }
        else {
            // empty request. throw this away.
            close_connection(client);
        }
    }
}


static void
on_input_accepted(struct ev_loop* loop, struct ev_io* input_stream, const int revents) {
    int client_fd = get_client_fd(input_stream);
    DEBUG2("%d", client_fd);
    if(client_fd < 0) {
        DEBUG2("Invalid fd %d", client_fd);
        return;
    }

    struct Client* client = malloc(sizeof(struct Client));
    if(client == NULL)
        return;

    client->fd = client_fd;

    if (set_nonblock(client->fd) < 0)
        ERROR("Could not set fd nonblocking");

    DEBUG2("Starting libev loop %d", (int)loop);

    ev_io_init(&client->ev_read, on_client_connected, client->fd, EV_READ);
    ev_io_start(loop, &client->ev_read);

    return;
}


static void
bjoern_loop_new() {
    ev_io accept_listener;
    struct ev_loop* loop = ev_default_loop(0);

    ev_io_init(&accept_listener, &on_input_accepted, 1 /* STDIN */, EV_READ);
    ev_io_start(loop, &accept_listener);

    ev_loop(loop, 0);
}


int
main(int argc, char** argv) {
    bjoern_loop_new();
    return 0;
}
