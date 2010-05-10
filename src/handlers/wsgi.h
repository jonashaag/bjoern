struct wsgi_handler_data {
    PyObject* status;
    PyObject* headers;
    const char* body;
    size_t body_length;
    Route* route;
};

static handler_initialize   wsgi_handler_initialize;
static handler_write        wsgi_handler_write;
static handler_finalize     wsgi_handler_finalize;
