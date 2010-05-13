static bool
raw_handler_initialize(Transaction* transaction)
{
    transaction->handler_data.raw.response_length = strlen(transaction->handler_data.raw.response);
    return true;
}

static response_status
raw_handler_write(Transaction* transaction)
{
    ssize_t bytes_sent = write(
        transaction->client_fd,
        transaction->handler_data.raw.response,
        transaction->handler_data.raw.response_length
    );
    if(bytes_sent == -1) {
        /* An error occured. */
        if(errno == EAGAIN)
            return RESPONSE_NOT_YET_FINISHED; /* Try again next time. */
        else
            ; /* TODO: Machwas! */
    }
    return RESPONSE_FINISHED;
}

static inline void
raw_handler_finalize(Transaction* transaction)
{
}
