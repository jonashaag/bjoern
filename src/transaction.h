struct _Transaction {
    int client_fd;

    ev_io read_watcher;
    size_t read_seek;

    /* TODO: put request_* into a seperate data structure. */
    bjoern_http_parser* request_parser;
    http_method request_method;

    PyObject* wsgi_environ;

    ev_io write_watcher;

    handler_write writer;
    handler_finalize finalizer;
    request_handler_data request_handler;
};

static Transaction* Transaction_new();
#define Transaction_free(t) do { \
                                free(t->request_parser); \
                                free(t); \
                            } while(0)
