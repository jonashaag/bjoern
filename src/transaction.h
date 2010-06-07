struct _Transaction {
    int client_fd;
    ev_io read_watcher;
    size_t read_seek;
    bjoern_http_parser* request_parser;
    enum http_method request_method;

    ev_io write_watcher;

    response_status (*handler_write)(Transaction*);
    bool handler_needs_gil;
    void (*handler_finalize)(Transaction*);
    union {
        wsgi_handler_data  wsgi;
        raw_handler_data   raw;
#ifdef WANT_CACHING
        cache_handler_data cache;
#endif
    } handler_data;
};

static Transaction* Transaction_new();
#define Transaction_free(t) do { \
                                free(t->request_parser); \
                                free(t); \
                            } while(0)
