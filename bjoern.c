#include "bjoern.h"

#define GET_CLIENT_FROM_EV_READ(evread_) ((struct Client*) (((char*)evread_) - offsetof(struct Client, ev_read)))
#define GET_CLIENT_FROM_EV_WRITE(evwrite_) ((struct Client*) (((char*)evwrite_) - offsetof(struct Client, ev_write)))
#define nalloc(x) calloc(1, x)
#define COMBINE_STATEMENTS(code) do {code;} while(0)
#define error(fmt, ...) fprintf(stderr, "ERROR: " fmt "\n", ## __VA_ARGS__)
#define die(exit_code, ...) COMBINE_STATEMENTS(error( __VA_ARGS__ ); exit(exit_code))
#ifdef DEBUG
#define debug_line() debug("Current line: %d in function %s", __LINE__, __FUNCTION__)
#define debug(fmt, ...) fprintf(stderr, "DEBUG: " fmt "\n", ## __VA_ARGS__)
#else
#define debug(...)
#endif

static char* hostname;
static int port;
static int sockfd;
int client_ids;
static struct ev_loop* doorkeeper;

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
get_client_fd(struct ev_io* evread) {
    struct sockaddr_in client_address;
    socklen_t client_address_length = sizeof(client_address);

    return accept(
        evread->fd,
        (struct sockaddr*)&client_address,
        &client_address_length
    );
}


static struct Client*
Client_new() {
    struct Client* client = nalloc(sizeof(struct Client));
    return client;
}

static void
Client_free(struct Client* client) {
    debug("freeing Client %d...", (int)client);
    if(!client) return;

    if(client->request_header_orig) free(client->request_header_orig);

    if(client->response) free(client->response);
}


static void
close_connection(struct Client* client) {
    close(client->fd);
    Client_free(client);
}


static void inline
client_write(struct Client* client, const char* output, const size_t output_length) {
    debug("Sending %d chars long response to client %ld", (int)output_length, client->id);
    send(client->fd, output, output_length, 0);
}

static void
start_write(struct ev_loop* loop, struct ev_io* evwrite, const int revents) {
    debug_line();
    struct Client* client = GET_CLIENT_FROM_EV_WRITE(evwrite);

    debug_line();

    if(!(revents & EV_WRITE))
        return;

    debug("Sending response to client %ld", client->id);
    client_write(client, DUMMY_RESPONSE, strlen(DUMMY_RESPONSE)-10);
    sleep(2);
    client_write(client, "1234567890", 10);
    ev_io_stop(loop, evwrite);
    close_connection(client);
}


static void
on_client_connected(struct ev_loop* loop, struct ev_io* evread, const int revents) {
    if(!(revents & EV_READ))
        return; // something went terribly wrong.

    struct Client* client = GET_CLIENT_FROM_EV_READ(evread);
    debug("Client %ld connected. (fd is %d)", client->id, evread->fd);

    size_t input_length = 0;
    char input_buffer[HTTP_REQUEST_BUFFER_LENGTH] = "";
    bool read_finished = false;

    input_length = recv(evread->fd, &input_buffer, HTTP_REQUEST_BUFFER_LENGTH, 0);
    debug("Got %d bytes input: %s", input_length, input_buffer);
    if((int)input_length < 0) {
        debug("Error receiving input, errno is %d", errno);
        ev_io_stop(loop, evread);
        close_connection(client);
        return;
        die(42, "Woa, wtf");
    }
    if((int)input_length == 0) {
        read_finished = true;
    }
    else {
        client->request_header = realloc(client->request_header, (client->request_seek + input_length + 1)*sizeof(char));
        client->request_header_orig = client->request_header;
        memcpy(client->request_header + client->request_seek, &input_buffer, input_length);
        client->request_seek += input_length;
        client->request_header[client->request_seek] = '\0';


        bool uses_carriage_returns = true;
        if(!(client->request_body = strstr(client->request_header, "\r\n\r\n"))) {
             client->request_body = strstr(client->request_header, "\n\n");
            if(!client->request_body)
                return;
            uses_carriage_returns = false;
        }

        int header_length = client->request_body - client->request_header;
        client->request_header[header_length] = '\0';

        char* content_lenght = strstr(client->request_header, "Content-Length: ");
        if (!content_lenght) {
            read_finished = true;
        } else {
            int body_length = strtol(content_lenght + /* Content-Length: */16, NULL, 10);
            if ((int)client->request_seek == body_length + (uses_carriage_returns ? 4:2) + header_length +1) {
                 read_finished = true;
                 client->request_body += (uses_carriage_returns ? 4:2); // to skip the \r\n\r\n
            }
        }
    }

    if(read_finished) {
        ev_io_stop(loop, evread);
        if(strlen(client->request_header)) {
            ev_io_init(&client->ev_write, &start_write, client->fd, EV_WRITE);
            ev_io_start(client->evloop, &client->ev_write);
            ev_loop(client->evloop, EVLOOP_NONBLOCK);
        } else {
            // empty request. throw this away.
            close_connection(client);
        }
    }
}


static void
bjoern_thread_run(struct Client* client) {
    debug("New thread with fd=%d", client->fd);
    client->evloop = ev_loop_new(EVFLAG_NOENV);
    ev_io_init(&client->ev_read, &on_client_connected, client->fd, EV_READ);
    ev_io_start(client->evloop, &client->ev_read);
    ev_loop(client->evloop, EVLOOP_NONBLOCK);
}


static void
on_input_accepted(struct ev_loop* loop, struct ev_io* evread, const int revents) {
    debug("Incoming fd: %d", evread->fd);
    int client_fd = get_client_fd(evread);
    if(client_fd < 0)
        return;

    struct Client* client = Client_new();
    if(client == NULL)
        return;

    client->fd = client_fd;
    client->id = ++client_ids;

    debug("New client %ld. fd is %d", client->id, client->fd);

    if(set_nonblock(client->fd) < 0)
        error("Could not set fd %d non blocking", client->fd);

    pthread_t thread_id;
    pthread_create(&thread_id, 0, (void*)&bjoern_thread_run, (void*)client);
}


static bool bjoern_init() {
    struct addrinfo hints;
    struct addrinfo* servinfo;
    struct addrinfo* p;
    int rv;
    char strport[7];
    int* throwaway;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    snprintf(strport, sizeof(strport), "%d", port);

    if ((rv = getaddrinfo(hostname, strport, &hints, &servinfo)) == -1)
        return false;

    // loop through all the results and bind to the first we can
    // TODO: woa that's ugly, don't use a loop here
    for(p = servinfo; p; p = p->ai_next) {
        if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
            debug("creating socket failed with %d", errno);
            continue;
        }

        if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &throwaway, sizeof(int)) == -1) {
            debug("setsockopt failed");
            return false;
        }

        if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
            close(sockfd);
            continue;
        }
        break;
    }

    if (!p)
        return false;

    freeaddrinfo(servinfo);

    if (listen(sockfd, SOCKET_MAX_CONNECTIONS) == -1) {
        error("listen failed");
        return false;
    }

    return true;
}


static void
bjoern_quit(struct ev_loop* loop) {
    debug("Shutting down bjoern. Goodbye!");
    ev_unloop(loop, EVUNLOOP_ALL);
}

static void on_SIGINT_received(struct ev_loop* loop, ev_signal *w, int revents) {
    bjoern_quit(loop);
}

int
main(int argc, char** argv) {
    hostname = "0.0.0.0";
    port = 8080;

    if(!bjoern_init())
        die(1, "Could initialize bjoern\n");
    else
        debug("Listening on %s:%d", hostname, port);

    debug("My PID is %d", getpid());


    /* Run the default watch/bind/listen loop */
    ev_io accept_listener;
    ev_signal SIGINT_listener;

    doorkeeper = ev_default_loop(0);

    ev_signal_init(&SIGINT_listener, &on_SIGINT_received, SIGINT);
    ev_signal_start(doorkeeper, &SIGINT_listener);

    ev_io_init(&accept_listener, &on_input_accepted, sockfd, EV_READ);
    ev_io_start(doorkeeper, &accept_listener);

    ev_loop(doorkeeper, 0);

    return 0;
}
