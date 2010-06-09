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
    void* body; /* PyObject* py_file on sendfile; else char* body */
    size_t body_length;
    bool use_sendfile;
    PyObject* dealloc_extra;
};

static Transaction* Transaction_new();
#define Transaction_free(t) do { \
                                free(t->request_parser); \
                                free(t); \
                            } while(0)
