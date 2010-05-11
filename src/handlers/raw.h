struct _raw_handler_data {
    const char* response;
    size_t response_length;
};

static bool raw_handler_initialize(Transaction*);
static response_status raw_handler_write(Transaction*);
static void raw_handler_finalize(Transaction*);
