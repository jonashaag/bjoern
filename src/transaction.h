struct _Transaction {
    int client_fd;
    ev_io read_watcher;
    size_t read_seek;
    bjoern_http_parser* request_parser;
    enum http_method request_method;

    ev_io write_watcher;

    PyObject* request_environ;
#ifdef WANT_ROUTING
    Route* route;
    PyObject* route_kwargs;
#endif
    PyObject* status;
    PyObject* headers;
    bool headers_sent;
    bool use_sendfile;
    PyObject* body;
    size_t body_length;
    const char* body_position;
};

static Transaction* Transaction_new();
#define Transaction_free(t) do { \
                                free(t->request_parser); \
                                free(t); \
                            } while(0)
