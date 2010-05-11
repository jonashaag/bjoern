struct _wsgi_handler_data {
    PyObject* request_environ;
#ifdef WANT_ROUTING
    Route* route;
#endif
    PyObject* status;
    PyObject* headers;
    PyObject* file; /* for file responses */
    PyObject* dealloc_extra;
    const char* body;
    size_t body_length;
    bool headers_sent;
};

static bool wsgi_handler_initialize(Transaction*);
static response_status  wsgi_handler_write(Transaction*);
static response_status  wsgi_handler_sendfile(Transaction*);
static void wsgi_handler_send_headers(Transaction*);
static void wsgi_handler_finalize(Transaction*);
