#define PARSER ((bjoern_http_parser*)parser)
#define STOP_PARSER(code) \
            do { \
                PARSER->exit_code = code; \
                return PARSER_EXIT; \
            } while(0)
#define WSGI_HANDLER_DATA (PARSER->transaction->handler_data.wsgi)
/*
    Initialize the http parser.
*/
static int
http_on_start_parsing(http_parser* parser)
{
    PARSER->header_name_start   = NULL;
    PARSER->header_name_length  = 0;
    PARSER->header_value_start  = NULL;
    PARSER->header_value_length = 0;

    return PARSER_CONTINUE;
}

/*
    Parsing is done, populate some WSGI `environ` keys and so on.
*/
static int
http_on_end_parsing(http_parser* parser)
{
    /* Set the REQUEST_METHOD: */
    PyObject* py_request_method;

    switch(parser->method) {
        case HTTP_GET:
            py_request_method = PYSTRING(GET);
            break;
        case HTTP_POST:
            py_request_method = PYSTRING(GET);
            break;
        default:
            /* Currently, only POST and GET is supported. Fail here. */
            STOP_PARSER(HTTP_NOT_IMPLEMENTED);
    }

    PyDict_SetItem(
        WSGI_HANDLER_DATA.request_environ,
        PYSTRING(REQUEST_METHOD),
        py_request_method
    );

    /* Set the CONTENT_TYPE, which is the same as HTTP_CONTENT_TYPE. */
    PyObject* content_type = PyDict_GetItem(WSGI_HANDLER_DATA.request_environ,
                                            PYSTRING(HTTP_CONTENT_TYPE));
    if(content_type) {
        PyDict_SetItem(WSGI_HANDLER_DATA.request_environ,
                       PYSTRING(CONTENT_TYPE), content_type);
    }

    /* Ensure have QUERY_STRING. */
    if(! PyDict_Contains(WSGI_HANDLER_DATA.request_environ, PYSTRING(QUERY_STRING)))
        PyDict_SetItem(WSGI_HANDLER_DATA.request_environ,
                       PYSTRING(QUERY_STRING), PyString_FromString(""));

    /* TODO: Set SERVER_NAME and SERVER_PORT. */
    /* TODO: wsgi.* */
    return PARSER_CONTINUE;
}

/*
    Get the request URL and decide what to do:
      * if compiled with caching support and the there's a response for that
        request URL, return it from the cache.
      * if not and routing is enabled, try to find the corresponding route
*/
static int
http_on_path(http_parser* parser, const char* path_start, size_t path_length)
{
#ifdef WANT_CACHING
    if(cache_has(path_start, path_length)) {
        ((char*)path_start)[path_length] = '\0'; /* <-- we can do this safely because we need nothing but the URL for the cache stuff */
        parser->data = (void*)path_start;
        /* Stop parsing here, we don't need any more information: */
        STOP_PARSER(USE_CACHE);
    }
#endif

    PyObject* py_path = PyString_FromStringAndSize(path_start, path_length);
    if(py_path == NULL)
        STOP_PARSER(HTTP_INTERNAL_SERVER_ERROR);

#ifdef WANT_ROUTING
    Route* route = NULL;
    PyObject* kwargs = NULL;
    get_route_for_url(py_path, &route, &kwargs);
    if(route == NULL) {
        /* TODO: user-defined 404 fallback callback? */
        Py_DECREF(py_path);
        STOP_PARSER(HTTP_NOT_FOUND);
    }
    WSGI_HANDLER_DATA.route = route;
    WSGI_HANDLER_DATA.route_kwargs = kwargs;
#endif

    /* Create a new response header dictionary. */
    WSGI_HANDLER_DATA.request_environ = PyDict_New();
    if(WSGI_HANDLER_DATA.request_environ == NULL)
        STOP_PARSER(HTTP_INTERNAL_SERVER_ERROR);

    PyDict_SetItem(WSGI_HANDLER_DATA.request_environ, PYSTRING(PATH_INFO), py_path);

    DEBUG("*** %s %s", (parser->method == HTTP_GET ? "GET" : "POST"), PyString_AS_STRING(py_path));

    return PARSER_CONTINUE;
}

/*
    Set the QUERY_STRING.
*/
static int
http_on_query(http_parser* parser, const char* query_start, size_t query_length)
{
    PyObject* py_tmp = PyString_FromStringAndSize(query_start, query_length);
    PyDict_SetItem(WSGI_HANDLER_DATA.request_environ, PYSTRING(QUERY_STRING), py_tmp);

    return PARSER_CONTINUE;
}

/*
    Transform the current header name to something WSGI (CGI) compatible, e.g.
        User-Agent => HTTP_USER_AGENT
    and store it in the `wsgi_environ` dictionary.
*/
static inline void store_current_header(bjoern_http_parser* parser)
{
    PyObject* header_name;
    PyObject* header_value;
    char* header_name_c; /* The C string behind `header_name` */

    size_t name_length = parser->header_name_length + strlen("HTTP_");
    header_name = PyString_FromStringAndSize(NULL /* empty string */, name_length);
    header_name_c = PyString_AS_STRING(header_name);

    bjoern_strcpy(&header_name_c, "HTTP_");
    bjoern_http_to_wsgi_header(header_name_c, parser->header_name_start,
                                              parser->header_name_length);

    header_value = PyString_FromStringAndSize(parser->header_value_start,
                                              parser->header_value_length);
    PyDict_SetItem(WSGI_HANDLER_DATA.request_environ, header_name, header_value);

    Py_DECREF(header_name);
    Py_DECREF(header_value);
}


static int
http_on_header_name(http_parser* parser, const char* header_start, size_t header_length)
{
    if(PARSER->header_value_start) {
        /* We have a name/value pair to store, so do so. */
        store_current_header(PARSER);
        goto start_new_header;
    }
    if(PARSER->header_name_start) {
        /*  We already have a pointer to the header, so update the length. */
        /* TODO: Documentation */
        PARSER->header_name_length =
            (header_start - PARSER->header_name_start) + header_length;
        return PARSER_CONTINUE;
    }
    else {
        goto start_new_header;
    }

/* Start a new header. */
start_new_header:
    PARSER->header_name_start   = header_start;
    PARSER->header_name_length  = header_length;
    PARSER->header_value_start  = NULL;
    PARSER->header_value_length = 0;

    return PARSER_CONTINUE;
}

static int
http_on_header_value(http_parser* parser, const char* value_start, size_t value_length)
{
    if(PARSER->header_value_start) {
        /* We already have a value pointer, so update the length. */
        PARSER->header_value_length =
            (value_start - PARSER->header_value_start) + value_length;
    }
    else {
        /* Start new value. */
        PARSER->header_value_start = value_start;
        PARSER->header_value_length = value_length;
    }

    return PARSER_CONTINUE;
}



/* TODO: Implement with StringIO or something like that. */
static int
http_on_body(http_parser* parser, const char* body, size_t body_length)
{
    return PARSER_CONTINUE;
}

/* TODO: Decide what to do with this one. */
static int
http_on_fragment(http_parser* parser, const char* fragment_start, size_t fragment_length)
{
    return PARSER_CONTINUE;
}

static int
http_on_url(http_parser* parser, const char* url_start, size_t url_length)
{
    return PARSER_CONTINUE;
}

static int
http_on_headers_complete(http_parser* parser)
{
    return PARSER_CONTINUE;
}


http_parser_settings
parser_settings = {
    http_on_start_parsing,      /* http_cb      on_message_begin; */
    http_on_path,               /* http_data_cb on_path; */
    http_on_query,              /* http_data_cb on_query_string; */
    http_on_url,                /* http_data_cb on_url; */
    http_on_fragment,           /* http_data_cb on_fragment; */
    http_on_header_name,        /* http_data_cb on_header_field; */
    http_on_header_value,       /* http_data_cb on_header_value; */
    http_on_headers_complete,   /* http_cb      on_headers_complete; */
    http_on_body,               /* http_data_cb on_body; */
    http_on_end_parsing,        /* http_cb      on_message_complete; */
};

#undef PARSER
#undef WSGI_HANDLER_DATA
