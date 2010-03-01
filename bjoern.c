#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <stddef.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <fcntl.h>
#include <ev.h>

#define HTTP_REQUEST_BUFFER_LENGTH  1024
#define HTTP_RESPONSE_BUFFER_LENGTH 1024
#define SOCKET_MAX_CONNECTIONS 1024

#define GET_CLIENT_FROM_EV_IO(EV_IO_S, EV_IO_F) (struct Client*) (((char*)EV_IO_S) - offsetof(struct Client, EV_IO_F))

#define DEBUG_ON
#ifdef DEBUG_ON
#define DEBUG(X) fprintf(stdout, "DEBUG: " X "\n")
#define DEBUG2(X, Y) fprintf(stdout, "DEBUG: " X "\n", Y)
#define DEBUG3(X, Y, Z) fprintf(stdout, "DEBUG: " X "\n", Y, Z)
#else
#define DEBUG(x)
#endif
#define ERROR(X) fprintf(stdout, "ERROR: " X "\n")
#define ERROR2(X, Y) fprintf(stdout, "ERROR: " X "\n", Y)
#define PRINT_FUNC fprintf(stdout, "__FUNCTION__ = %s\n", __FUNCTION__);


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

#define RESET_CLIENT(client) \
    client->input_position = 0; \
    client->header = NULL; \
    client->body   = NULL;

#undef RESET_CLIENT
#define RESET_CLIENT(x)


static char* hostname;
static int port;
static int sockfd;


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
    struct sockaddr_in client_address;
    socklen_t client_address_length = sizeof(client_address);

    return accept(
        input_stream->fd,
        (struct sockaddr*)&client_address,
        &client_address_length
    );
}

#if 0
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
#endif



static void
close_connection(struct Client *client) {
    DEBUG("CLOSING CONNECTION");
    free(client->header);
    close(client->fd);
    free(client);
}


static void
client_write(struct Client* client, const char* output, int output_length) {
    PRINT_FUNC
    int sent_output;
    while(output_length) {
        DEBUG("WRITE.");
        sent_output = (int)write(client->fd, output, HTTP_RESPONSE_BUFFER_LENGTH);
        if(sent_output < 0) {
            DEBUG("<0");
            return;
        }
        else if(sent_output == 0) {
            DEBUG("=0");
            return;
        }
        else {
            DEBUG(">0");
            output += sent_output;
            output_length -= sent_output;
        }
    }
}

static void
start_write(struct ev_loop* loop, struct ev_io* input_stream, const int revents) {
    PRINT_FUNC
    struct Client* client = GET_CLIENT_FROM_EV_IO(input_stream, ev_write);
    if(!(revents & EV_WRITE))
        return;
    #define S "HTTP/1.1 200 OK\r\nServer: bjoern 0.1\r\nContent-Length: 9\r\nContent-Type: text/plain\r\nConnection: close\r\n\r\nHallo Welt"
    client_write(client, S, strlen(S));
    ev_io_stop(loop, input_stream);
    close_connection(client);
}


static void
on_client_connected(struct ev_loop* loop, struct ev_io* input_stream, const int revents) {
    PRINT_FUNC
    if(!(revents & EV_READ))
        return; // something went terribly wrong.

    struct Client* client = GET_CLIENT_FROM_EV_IO(input_stream, ev_read);
    size_t input_length;
    char input_buffer[HTTP_REQUEST_BUFFER_LENGTH];
    bool read_finished;

    input_length = read(client->fd, &input_buffer, HTTP_REQUEST_BUFFER_LENGTH);
    if(input_length < 0) {
        DEBUG("INPUT LENGTH < 0");
        ev_io_stop(loop, input_stream);
        close_connection(client);
        return;
    }
    else if(input_length == 0) {
        DEBUG("INPUT LENGTH = 0");
        read_finished = true;
    }
    else {
        DEBUG("READING HEADER");
        client->header = realloc(client->header, (client->input_position + input_length + 1)*sizeof(char));
        memcpy(client->header + client->input_position, &input_buffer, input_length);
        client->input_position += input_length;
        client->header[client->input_position] = '\0';


        bool uses_carriage_returns = true;
        if(!(client->body = strstr(client->header, "\r\n\r\n"))) {
            client->body = strstr(client->header, "\n\n");
            if(!client->body) {
                ERROR("Client sent no body or header (didn't find any \\n\\n or \\r\\n\\r\\n)");
                return;
            }
            uses_carriage_returns = false;
            DEBUG("Having no carriage returns");
        } else {
            DEBUG("HAVING carriage returns");
        }
        DEBUG2("Body: %d", (int)client->body);

        int header_length = client->body - client->header;
        client->header[header_length] = '\0';
        DEBUG2("Header length: %d", header_length);

        char *content_lenght = strstr(client->header, "Content-Length: ");
        if (!content_lenght) {
            DEBUG("NO CONTENT LENGTH!");
            read_finished = true;
        } else {
            int body_length = strtol(content_lenght + /* Content-Length: */16, NULL, 10);
            DEBUG2("Body length: %d", body_length);
            if ((int)client->input_position == body_length + (uses_carriage_returns ? 4:2) + header_length +1) {
                 read_finished = true;
                 client->body += (uses_carriage_returns ? 4:2); // to skip the \r\n\r\n
            }
        }
    }

    if(read_finished) {
        DEBUG("READ FINISHED");
        DEBUG2("HEADER IS %s", client->header);
        DEBUG2("BODY IS %s", client->body);
        ev_io_stop(loop, input_stream);
        if(strlen(client->header)) {
            ev_io_init(&client->ev_write, &start_write, client->fd, EV_WRITE);
            ev_io_start(loop, &client->ev_write);
        }
        else {
            // empty request. throw this away.
            ERROR("EMPTY REQUEST");
            close_connection(client);
        }
    }
}


static void
on_input_accepted(struct ev_loop* loop, struct ev_io* input_stream, const int revents) {
    PRINT_FUNC
    DEBUG("Loop accepted");
    int client_fd = get_client_fd(input_stream);
    DEBUG2("client_fd %d", client_fd);
    if(client_fd < 0) {
        DEBUG2("Invalid fd %d", client_fd);
        return;
    }

    struct Client* client = malloc(sizeof(struct Client));
    RESET_CLIENT(client);
    if(client == NULL) {
        ERROR("client alloc failed");
        return;
    }

    client->fd = client_fd;

    if (set_nonblock(client->fd) < 0)
        ERROR("Could not set fd nonblocking");

    DEBUG2("Starting libev loop %d", (int)loop);

    ev_io_init(&client->ev_read, on_client_connected, client->fd, EV_READ);
    ev_io_start(loop, &client->ev_read);

    sleep(1);
}


static bool bjoern_init() {
    PRINT_FUNC
    struct addrinfo hints;
    struct addrinfo* servinfo;
    struct addrinfo* p;
    int rv;
    //int* yes;
    char strport[7];
    int* throwaway;

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE; // use my IP

    snprintf(strport, sizeof (strport), "%d", port);

    if ((rv = getaddrinfo(hostname, strport, &hints, &servinfo)) == -1) {
        ERROR2("getaddrinfo: %s\n", gai_strerror(rv));
        return false;
    }

    // loop through all the results and bind to the first we can
    // TODO: woa that's ugly, don't use a loop here
    for(p = servinfo; p; p = p->ai_next) {
        if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
            ERROR("server: socket");
            continue;
        }

        if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &throwaway, sizeof(int)) == -1) {
            ERROR("setsockopt");
            return false;
        }

        if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
            close(sockfd);
            ERROR("server: bind");
            continue;
        }

        break;
    }

    if (!p)  {
        ERROR("server: failed to bind");
        return false;
    }

    freeaddrinfo(servinfo); // all done with this structure

    if (listen(sockfd, SOCKET_MAX_CONNECTIONS) == -1) {
        ERROR("listen");
        return false;
    }

    return true;
}


static void
bjoern_loop_new() {
    PRINT_FUNC
    DEBUG("New bjoern loop");
    ev_io accept_listener;
    struct ev_loop* loop = ev_default_loop(0);

    ev_io_init(&accept_listener, &on_input_accepted, sockfd, EV_READ);
    ev_io_start(loop, &accept_listener);

    DEBUG("Starting loop");
    ev_loop(loop, 0);
}


int
main(int argc, char** argv) {
    PRINT_FUNC
    hostname = "0.0.0.0";
    port = 8080;
    DEBUG3("Trying to initialize bjoern on %s:%d", hostname, port);
    if(!bjoern_init()) {
        ERROR("INIT FAILED");
        return 1;
    }
    DEBUG2("sockfd=%d", sockfd);
    bjoern_loop_new();
    return 0;
}
