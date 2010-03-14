#include "bjoern.h"

#define NULLALLOC(size)     calloc(1, size);
#define MAX(a, b)           ((a) > (b) ? (a) : (b))
#define OFFSETOF(member, ptr, type) ((type*) (((char*)ptr) - offsetof(type, member)))

#ifdef DEBUG_ON
#define DEBUG(...)          fprintf(stdout, __VA_ARGS__)
#else
#define DEBUG(...)
#endif


struct Transaction* Transaction_new()
{
    return (struct Transaction*)NULLALLOC(sizeof(struct Transaction));
}

void Transaction_free(struct Transaction* transaction)
{
    free(transaction->request);
    free(transaction->response);
    free(transaction);
}


static void while_sock_canwrite(EV_LOOP mainloop, ev_io *write_watcher_, int revents)
{
    struct Transaction* transaction = OFFSETOF(write_watcher, write_watcher_, struct Transaction);
}

static void on_sock_read(EV_LOOP mainloop, ev_io *read_watcher_, int revents)
{
    struct Transaction* transaction = OFFSETOF(read_watcher, read_watcher_, struct Transaction);
    ev_io_init(&transaction->write_watcher, &while_sock_canwrite, transaction->client_fd, EV_WRITE);
    ev_io_start(mainloop, &transaction->write_watcher);
}


static void on_sock_accept(EV_LOOP mainloop, ev_io *accept_watcher, int revents)
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

    /* Set the file descriptor non-blocking. */
    fcntl(transaction->client_fd, F_SETFL, MAX(fcntl(transaction->client_fd, F_GETFL, 0), 0) | O_NONBLOCK);

    /* Run the read-watch loop. */
    ev_io_init(&transaction->read_watcher, &on_sock_read, transaction->client_fd, EV_READ);
    ev_io_start(mainloop, &transaction->read_watcher);
}

static void on_sigint_received(EV_LOOP mainloop, ev_signal *signal, int revents)
{
    DEBUG("SIGINT received, shutting down. Goodbye!");
    ev_unloop(mainloop, EVUNLOOP_ALL);
}

int main(int argcount, char** args)
{
    EV_LOOP     mainloop;
    ev_signal   sigint_watcher;
    ev_io       accept_watcher;

    mainloop = ev_default_loop(0);

    /* Run the SIGINT-watch loop. */
    ev_signal_init(&sigint_watcher, &on_sigint_received, SIGINT);
    ev_signal_start(mainloop, &sigint_watcher);

    /* Run the accept-watch loop. */
    ev_io_init(&accept_watcher, &on_sock_accept, GET_SOCKFD_USING_MAGIC(), EV_READ);
    ev_io_start(mainloop, &accept_watcher);

    ev_loop(mainloop, 0);

    return 0;
}
