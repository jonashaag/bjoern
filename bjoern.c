#include "bjoern.h"

#define GET_CLIENT_FROM_EV_IO(EV_IO_S, EV_IO_F) (struct Client*) (((char*)EV_IO_S) - offsetof(struct Client, EV_IO_F))

#define COMBINE_STATEMENTS(code) do {code;} while(0)
#define error(...) fprintf(stderr, "ERROR: " __VA_ARGS__ )
#define die(exit_code, ...) COMBINE_STATEMENTS(error( __VA_ARGS__ ); exit(exit_code))
#ifdef DEBUG
#define debug(...) fprintf(stderr, "DEBUG: " __VA_ARGS__ )
#else
#define debug(...)
#endif

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


static void
close_connection(struct Client *client) {
    free(client->header);
    close(client->fd);
    free(client);
}


static void inline
client_write(struct Client* client, const char* output, const size_t output_length) {
    send(client->fd, output, output_length, 0);
}

static void
start_write(struct ev_loop* loop, struct ev_io* input_stream, const int revents) {
    struct Client* client = GET_CLIENT_FROM_EV_IO(input_stream, ev_write);
    if(!(revents & EV_WRITE))
        return;
    client_write(client, DUMMY_RESPONSE, strlen(DUMMY_RESPONSE));
    ev_io_stop(loop, input_stream);
    close_connection(client);
}


static void
on_client_connected(struct ev_loop* loop, struct ev_io* input_stream, const int revents) {
    if(!(revents & EV_READ))
        return; // something went terribly wrong.

    struct Client* client = GET_CLIENT_FROM_EV_IO(input_stream, ev_read);
    size_t input_length = 0;
    char input_buffer[HTTP_REQUEST_BUFFER_LENGTH] = "";
    bool read_finished = false;

    input_length = recv(client->fd, &input_buffer, HTTP_REQUEST_BUFFER_LENGTH, 0);
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


        bool uses_carriage_returns = true;
        if(!(client->body = strstr(client->header, "\r\n\r\n"))) {
            client->body = strstr(client->header, "\n\n");
            if(!client->body)
                return;
            uses_carriage_returns = false;
        }

        int header_length = client->body - client->header;
        client->header[header_length] = '\0';

        char *content_lenght = strstr(client->header, "Content-Length: ");
        if (!content_lenght) {
            read_finished = true;
        } else {
            int body_length = strtol(content_lenght + /* Content-Length: */16, NULL, 10);
            if ((int)client->input_position == body_length + (uses_carriage_returns ? 4:2) + header_length +1) {
                 read_finished = true;
                 client->body += (uses_carriage_returns ? 4:2); // to skip the \r\n\r\n
            }
        }
    }

    if(read_finished) {
        ev_io_stop(loop, input_stream);
        if(strlen(client->header)) {
            ev_io_init(&client->ev_write, &start_write, client->fd, EV_WRITE);
            ev_io_start(loop, &client->ev_write);
        } else {
            // empty request. throw this away.
            close_connection(client);
        }
    }
}


static void
on_input_accepted(struct ev_loop* loop, struct ev_io* input_stream, const int revents) {
    int client_fd = get_client_fd(input_stream);
    if(client_fd < 0)
        return;

    struct Client* client = malloc(sizeof(struct Client));
    if(client == NULL)
        return;
    reset_client(client);

    client->fd = client_fd;

    set_nonblock(client->fd);

    ev_io_init(&client->ev_read, on_client_connected, client->fd, EV_READ);
    ev_io_start(loop, &client->ev_read);
}


static bool bjoern_init() {
    struct addrinfo hints;
    struct addrinfo* servinfo;
    struct addrinfo* p;
    int rv;
    char strport[7];
    int* throwaway;

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    snprintf(strport, sizeof (strport), "%d", port);

    if ((rv = getaddrinfo(hostname, strport, &hints, &servinfo)) == -1)
        return false;

    // loop through all the results and bind to the first we can
    // TODO: woa that's ugly, don't use a loop here
    for(p = servinfo; p; p = p->ai_next) {
        if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1)
            continue;

        if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &throwaway, sizeof(int)) == -1)
            return false;

        if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
            close(sockfd);
            continue;
        }

        break;
    }

    if (!p)
        return false;

    freeaddrinfo(servinfo); // all done with this structure

    if (listen(sockfd, SOCKET_MAX_CONNECTIONS) == -1)
        return false;

    return true;
}


static void on_SIGINT_received(struct ev_loop *loop, ev_signal *w, int revents) {

    ev_unloop(loop, EVUNLOOP_ALL);
}


static void
bjoern_loop_new() {
    ev_io accept_listener;
    ev_signal SIGINT_listener;

    struct ev_loop* loop = ev_default_loop(0);

    ev_signal_init(&SIGINT_listener, &on_SIGINT_received, SIGINT);
    ev_signal_start(loop, &SIGINT_listener);

    ev_io_init(&accept_listener, &on_input_accepted, sockfd, EV_READ);
    ev_io_start(loop, &accept_listener);

    ev_loop(loop, 0);
}


int
main(int argc, char** argv) {
    hostname = "0.0.0.0";
    port = 8080;
    if(!bjoern_init())
        die(1, "Could initialize bjoern\n");
    bjoern_loop_new();
    return 0;
}
