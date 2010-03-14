#include "bjoern.h"
#include "http.c"

#define NULLALLOC(size)     calloc(1, size);
#define MAX(a, b)           ((a) > (b) ? (a) : (b))
#define OFFSETOF(member, ptr, type) ((type*) (((char*)ptr) - offsetof(type, member))) /* TODO: Rename. */

#ifdef DEBUG_ON
#define DEBUG(s, ...)       fprintf(stdout, s"\n", ## __VA_ARGS__ ); fflush(stdout)
#else
#define DEBUG(...)
#endif
#define ERROR(s, ...)       fprintf(stderr, s"\n", ## __VA_ARGS__ ); fflush(stderr)


static struct Transaction* Transaction_new()
{
    return (struct Transaction*)NULLALLOC(sizeof(struct Transaction));
}

static void Transaction_free(struct Transaction* transaction)
{
    free(transaction->request);
    free(transaction->response);
    free(transaction);
}


static void while_sock_canwrite(EV_LOOP mainloop, ev_io* write_watcher_, int revents)
{
    DEBUG("Write start.");

    struct Transaction* transaction = OFFSETOF(write_watcher, write_watcher_, struct Transaction);

    #define MSG "HTTP/1.1 200 Alles ok"
    write(transaction->client_fd, MSG, strlen(MSG));

    ev_io_stop(mainloop, &transaction->write_watcher);
    close(transaction->client_fd);
    Transaction_free(transaction);

    DEBUG("Write end.");
}


/*
    TODO: Make sure this function is very, very fast as it is called many times.
*/
static void on_sock_read(EV_LOOP mainloop, ev_io* read_watcher_, int revents)
{
    /* Check whether we can still read. */
    if(!(revents & EV_READ))
        return;

    struct Transaction* transaction = OFFSETOF(read_watcher, read_watcher_, struct Transaction);

    /* Read the whole body. */
    char    read_buffer[READ_BUFFER_SIZE] = {0};
    size_t  bytes_read;
    bytes_read = read(transaction->client_fd, read_buffer, READ_BUFFER_SIZE);

    DEBUG("READ: %d", (int)bytes_read);

    if(bytes_read < 0)
        /* An error happened. TODO: Limit retries? */
        return;

    if(bytes_read == 0) goto read_finished;
    else {
        DEBUG("Read %d bytes from client %d.", bytes_read, transaction->client_fd);
        transaction->request = realloc(transaction->request,
                                       (transaction->read_seek + bytes_read + 1) * sizeof(char));
        memcpy(transaction->request + transaction->read_seek, read_buffer, bytes_read);
        transaction->read_seek += bytes_read;
        transaction->request[transaction->read_seek] = '\0';


        /* TODO: THIS.SECTION.NEEDS.A.COMPLETE.REWRITE. */
        int body_length = http_split_body(transaction->request,
                                          transaction->read_seek,
                                          transaction->request_body);

        DEBUG("Body length is %d", body_length);
        if(body_length) {
            /* We have a body, so there's a chance that we have a
               'Content-Length' header. Go search it. */
            int request_header_length = transaction->request_body - transaction->request;

            int proclaimed_body_length = http_get_content_length(transaction->request);
            if(proclaimed_body_length && transaction->read_seek >= (request_header_length + proclaimed_body_length)) {
                /* We have the wohle body, stop reading. */
                goto read_finished;
            }
            else
                /* We don't have the whole body yet, don't stop reading. */
                return;

        }
        else
            /* We haven't got any body, so stop reading here. (See Footnote 0) */
            goto read_finished;
    }

read_finished:
    DEBUG("Read finished! Body is \n%s", transaction->request);
    /* Stop the read loop, we're done reading. */
    ev_io_stop(mainloop, &transaction->read_watcher);

    /* Run the write-watch loop that notifies us when we can write to the socket. */
    ev_io_init(&transaction->write_watcher, &while_sock_canwrite, transaction->client_fd, EV_WRITE);
    ev_io_start(mainloop, &transaction->write_watcher);
}


static void on_sock_accept(EV_LOOP mainloop, ev_io* accept_watcher, int revents)
{
    struct Transaction* transaction = Transaction_new();

    /* Accept the socket connection. */
    struct sockaddr_in client_address;
    socklen_t client_address_length = sizeof(client_address);
    transaction->client_fd = accept(
        accept_watcher->fd,
        (struct sockaddr *)&client_address,
        &client_address_length
    );

    if(transaction->client_fd < 0) return; /* ERROR!1!1! */

    DEBUG("Accepted client on %d.", transaction->client_fd);

    /* Set the file descriptor non-blocking. */
    fcntl(transaction->client_fd, F_SETFL, MAX(fcntl(transaction->client_fd, F_GETFL, 0), 0) | O_NONBLOCK);

    /* Run the read-watch loop. */
    ev_io_init(&transaction->read_watcher, &on_sock_read, transaction->client_fd, EV_READ);
    ev_io_start(mainloop, &transaction->read_watcher);
}

static void on_sigint_received(EV_LOOP mainloop, ev_signal *signal, int revents)
{
    DEBUG("Received SIGINT, shutting down. Goodbye!");
    ev_unloop(mainloop, EVUNLOOP_ALL);
}


static int init_socket()
{
    int sockfd = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
    if(socket < 0) return -1;

    union _sockaddress {
        struct sockaddr    sockaddress;
        struct sockaddr_in sockaddress_in;
    } server_address;

    server_address.sockaddress_in.sin_family = PF_INET;
    inet_aton("0.0.0.0", &server_address.sockaddress_in.sin_addr);
    server_address.sockaddress_in.sin_port = htons(8080);

    int st;
    st = bind(sockfd, &server_address.sockaddress, sizeof(server_address));
    if(st < 0) return -2;

    st = listen(sockfd, MAX_LISTEN_QUEUE_LENGTH);
    if(st < 0) return -3;

    return sockfd;
}

static void handle_socket_error(int error_num)
{
    switch(error_num) {
        case -1:
            ERROR("Could not create socket().");
            break;
        case -2:
            ERROR("Could not bind().");
            break;
        case -3:
            ERROR("Could not listen().");
            break;
    }
    exit(1);
}

int main(int argcount, char** args)
{
    int sockfd = init_socket();
    if(sockfd < 0) handle_socket_error(sockfd);

    EV_LOOP     mainloop;
    ev_signal   sigint_watcher;
    ev_io       accept_watcher;

    mainloop = ev_default_loop(0);

    /* Run the SIGINT-watch loop. */
    ev_signal_init(&sigint_watcher, &on_sigint_received, SIGINT);
    ev_signal_start(mainloop, &sigint_watcher);

    /* Run the accept-watch loop. */
    ev_io_init(&accept_watcher, &on_sock_accept, sockfd, EV_READ);
    ev_io_start(mainloop, &accept_watcher);

    ev_loop(mainloop, 0);

    return 0;
}


/*
    -----------
     Footnotes
    -----------

    0.  Actually it's possible that the header wasn't completely sent after one
        `read`, but that should be so uncommon that we can ignore that case.

*/
