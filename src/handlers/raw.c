static bool
cache_handler_initialize(Transaction* transaction)
{
    transaction->request_handler->response_length = strlen(transaction->request_handler->response);
}

static response_status
cache_handler_write(Transaction* transaction)
{
    ssize_t bytes_sent = write(
        transaction->client_fd,
        transaction->request_handler.response
        transaction->request_handler.response_length
    );
    if(bytes_sent == -1) {
        /* An error occured. */
        if(errno == EAGAIN)
            return RESPONSE_NOT_YET_FINISHED; /* Try again next time. */
        else
            DO_NOTHING; /* TODO: Machwas! */
    }
    return RESPONSE_FINISHED;
}

static void
cache_handler_finalize(Transaction* transaction)
{
}
