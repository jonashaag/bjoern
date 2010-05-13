struct _wsgi_data {
    PyObject* status;
    PyObject* headers;
    const char* body;
    size_t body_length;
    bool headers_sent;
};

struct _wsgi_sendfile_data {
    PyFileObject* file;
    size_t file_size;
    int file_descriptor;
};


struct _wsgi_handler_data {
    PyObject* request_environ;
#ifdef WANT_ROUTING
    Route* route;
    PyObject* route_kwargs;
#endif
    union {
        struct _wsgi_data wsgi;
        struct _wsgi_sendfile_data sendfile;
    } data;
    PyObject* dealloc_extra;
};


static bool wsgi_handler_initialize(Transaction*);
static bool wsgi_handler_sendfile_init(Transaction*, PyFileObject*);
static response_status wsgi_handler_sendfile(Transaction*);
static response_status wsgi_handler_write(Transaction*);
static void wsgi_handler_send_headers(Transaction*);
static void wsgi_handler_finalize(Transaction*);
