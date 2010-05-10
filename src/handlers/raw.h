struct raw_handler_data {
    const char* response;
};

static handler_initialize   raw_handler_initialize;
static handler_write        raw_handler_write;
static handler_finalize     raw_handler_finalize;
