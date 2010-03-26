#include "bjoern.h"
#include "http.c"

/* TODO: Use global parser if possible */


static struct http_parser_settings
  parser_settings = {
    http_on_start_parsing,      /* http_cb      on_message_begin; */
    http_set_path,              /* http_data_cb on_path; */
    http_set_query,             /* http_data_cb on_query_string; */
    http_set_url,               /* http_data_cb on_url; */
    http_set_fragment,          /* http_data_cb on_fragment; */
    http_set_header,            /* http_data_cb on_header_field; */
    http_set_header_value,      /* http_data_cb on_header_value; */
    http_on_headers_complete,   /* http_cb      on_headers_complete; */
    http_set_body,              /* http_data_cb on_body; */
    http_on_end_parsing,        /* http_cb      on_message_complete; */
};

static int NUMS = 1;

static TRANSACTION* Transaction_new()
{
    TRANSACTION* transaction = malloc(sizeof(TRANSACTION));

    /* Initialize the http parser. */
    NENSURE(transaction->request_parser = malloc(sizeof(BJPARSER)));
    http_parser_init((PARSER*)transaction->request_parser, HTTP_REQUEST);

    /* Initialize the Python headers dictionary. */
    NENSURE(transaction->request_headers = PyDict_New());
    Py_INCREF(transaction->request_headers);

    transaction->num = NUMS++;

    return transaction;
}

static void Transaction_free(TRANSACTION* transaction)
{
    /* TODO: Free PyObjects here? */
    //free(transaction->response);
    free(transaction->request_parser);
    free(transaction);
}


static void while_sock_canwrite(EV_LOOP mainloop, ev_io* write_watcher_, int revents)
{
    DEBUG("Write start.");

    TRANSACTION* transaction = OFFSETOF(write_watcher, write_watcher_, TRANSACTION);

    #define MSG "HTTP/1.1 200 Alles ok"
    write(transaction->client_fd, MSG, strlen(MSG));

    ev_io_stop(mainloop, &transaction->write_watcher);
    close(transaction->client_fd);
    Transaction_free(transaction);

    DEBUG("Write end.\n\n");
}

/*
    TODO: Make sure this function is very, very fast as it is called many times.
*/
static void on_sock_read(EV_LOOP mainloop, ev_io* read_watcher_, int revents)
{
    /* Check whether we can still read. */
    VENSURE(revents & EV_READ);

    TRANSACTION* transaction = OFFSETOF(read_watcher, read_watcher_, TRANSACTION);

    /* Read the whole body. */
    char    read_buffer[READ_BUFFER_SIZE] = {0};
    size_t  bytes_read;
    size_t  bytes_parsed;
    bytes_read = read(transaction->client_fd, read_buffer, READ_BUFFER_SIZE);

    if(bytes_read < 0)
        /* An error happened. TODO: Limit retries? */
        return;

    if(bytes_read == 0) goto read_finished;
    else {
        bytes_parsed = http_parser_execute(
            (PARSER*)transaction->request_parser,
            parser_settings,
            read_buffer,
            bytes_read
        );

        DEBUG("Parsed %d bytes of %d", (int)bytes_read, (int)bytes_parsed);
    }

read_finished:
    /* Stop the read loop, we're done reading. */
    ev_io_stop(mainloop, &transaction->read_watcher);

    /* Run the write-watch loop that notifies us when we can write to the socket. */
    ev_io_init(&transaction->write_watcher, &while_sock_canwrite, transaction->client_fd, EV_WRITE);
    ev_io_start(mainloop, &transaction->write_watcher);
}


static void on_sock_accept(EV_LOOP mainloop, ev_io* accept_watcher, int revents)
{
    TRANSACTION* transaction = Transaction_new();

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


static void bjoern_exit(const int exit_code)
{
    close(sockfd);
    exit(exit_code);
}


static void on_sigint_received(EV_LOOP mainloop, ev_signal *signal, int revents)
{
    printf("Received SIGINT, shutting down. Goodbye!\n");
    ev_unloop(mainloop, EVUNLOOP_ALL);
    bjoern_exit(0);
}


static int init_socket()
{
    sockfd = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
    if(socket < 0) return EXIT_CODE_SOCKET_FAILED;

    union _sockaddress {
        struct sockaddr    sockaddress;
        struct sockaddr_in sockaddress_in;
    } server_address;

    server_address.sockaddress_in.sin_family = PF_INET;
    inet_aton("0.0.0.0", &server_address.sockaddress_in.sin_addr);
    server_address.sockaddress_in.sin_port = htons(8080);

    /* Set SO_REUSEADDR to make the IP address we used available for reuse. */
    int optval = true;
    setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));

    int st;
    st = bind(sockfd, &server_address.sockaddress, sizeof(server_address));
    if(st < 0) return EXIT_CODE_BIND_FAILED;

    st = listen(sockfd, MAX_LISTEN_QUEUE_LENGTH);
    if(st < 0) return EXIT_CODE_LISTEN_FAILED;

    return sockfd;
}

static void handle_socket_error(const int error_num)
{
    printf("Could not start bjoern: ");
    switch(error_num) {
        case EXIT_CODE_SOCKET_FAILED: printf("socket"); break;
        case EXIT_CODE_BIND_FAILED:   printf("bind");   break;
        case EXIT_CODE_LISTEN_FAILED: printf("listen"); break;
    };
    perror(" failed: ");
    bjoern_exit(error_num);
}

int main(int argcount, char** args)
{
    sockfd = init_socket();
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
