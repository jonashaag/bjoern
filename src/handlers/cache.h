struct _cache_handler_data {
    const char* cache_key;
};

static bool cache_has(const char* url, size_t url_length);
static bool cache_handler_initialize(Transaction*);
static response_status cache_handler_write(Transaction*);
static void cache_handler_finalize(Transaction*);
