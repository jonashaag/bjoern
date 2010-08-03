#include "parser.h"

#define PARSER ((Parser*)parser)
#define ENV_SETITEM(k, v) Environ_SetItem(PARSER->request->request_environ, k, v)
#define STOP_PARSER(code) do { parser->data = (void*)code; \
                               return PARSER_EXIT; } while(0)

http_parser_settings parser_settings;

Parser* Parser_new()
{
    Parser* parser = calloc(1, sizeof(Parser));
    http_parser_init((http_parser*)parser, HTTP_REQUEST);
    return parser;
}

void
Parser_execute(Parser* parser, c_char* input, c_size_t len)
{
    http_parser_execute((http_parser*)parser, &parser_settings, input, len);
}

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
    PyObject* http_method;
    /* Set the REQUEST_METHOD: */
    if(parser->method < sizeof(py_http_methods))
        http_method = py_http_methods[parser->method];
    else
        http_method = PyString_FromString(http_method_str(parser->method));

    ENV_SETITEM(
        _(REQUEST_METHOD),
        http_method
    );

    /* Set the CONTENT_TYPE, which is the same as HTTP_CONTENT_TYPE. */
    PyObject* content_type = PyDict_GetItem(PARSER->request->request_environ,
                                            _(HTTP_CONTENT_TYPE));
    if(content_type)
        ENV_SETITEM(
            _(CONTENT_TYPE),
            content_type
        );

    /* Ensure have QUERY_STRING. */
    if(! PyDict_Contains(PARSER->request->request_environ, _(QUERY_STRING)))
        ENV_SETITEM(
            _(QUERY_STRING),
            _static_empty_pystring
        );

    /* TODO: Set SERVER_NAME and SERVER_PORT. */
    /* TODO: wsgi.* */
    return PARSER_CONTINUE;
}

/*
    Get the request URL and decide what to do:
      * if routing is enabled, try to find the corresponding route
*/
static int
http_on_path(http_parser* parser, c_char* path_start, size_t path_length)
{
    PyObject* py_path = PyString_FromStringAndSize(path_start, path_length);
    if(py_path == NULL)
        STOP_PARSER(HTTP_INTERNAL_SERVER_ERROR);

#ifdef WANT_ROUTING
    get_route_for_url(
        py_path,
        &PARSER->request->route,
        &PARSER->request->route_kwargs
    );
    if(PARSER->request->route == NULL) {
        /* TODO: user-defined 404 fallback callback? */
        Py_DECREF(py_path);
        STOP_PARSER(HTTP_NOT_FOUND);
    }
#endif

    /* Create a new response header dictionary. */
    PARSER->request->request_environ = Environ_new();
    if(!PARSER->request->request_environ)
        STOP_PARSER(HTTP_INTERNAL_SERVER_ERROR);

    ENV_SETITEM(
        _(PATH_INFO),
        py_path
    );

    DEBUG("*** %s %s", (parser->method == HTTP_GET ? "GET" : "POST"), PyString_AS_STRING(py_path));

    return PARSER_CONTINUE;
}

/*
    Set the QUERY_STRING.
*/
static int
http_on_query(http_parser* parser, c_char* query_start, size_t query_length)
{
    PyObject* query = PyString_FromStringAndSize(query_start, query_length);
    if(query == NULL)
        STOP_PARSER(HTTP_INTERNAL_SERVER_ERROR);

    ENV_SETITEM(
        _(QUERY_STRING),
        query    
    );

    return PARSER_CONTINUE;
}

static int
http_on_header_name(http_parser* parser, c_char* header_start, c_size_t header_length)
{
    if(PARSER->header_value_start) {
        /* We have a name/value pair to store, so do so. */
        Environ_SetItemWithLength(
            PARSER->request->request_environ,
            PARSER->header_name_start,
            PARSER->header_name_length,
            PARSER->header_value_start,
            PARSER->header_value_length
        );
    } else if(PARSER->header_name_start) {
        /*  We already have a pointer to the header, so update the length. */
        /* TODO: Documentation */
        PARSER->header_name_length =
            (header_start - PARSER->header_name_start) + header_length;
        return PARSER_CONTINUE;
    }

    /* Start a new header. */
    PARSER->header_name_start   = header_start;
    PARSER->header_name_length  = header_length;
    PARSER->header_value_start  = NULL;
    PARSER->header_value_length = 0;

    return PARSER_CONTINUE;
}

static int
http_on_header_value(http_parser* parser, c_char* value_start, size_t value_length)
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
static inline int
http_on_body(http_parser* parser, c_char* body, size_t body_length)
{
    return PARSER_CONTINUE;
}

/* TODO: Decide what to do with this one. */
static inline int
http_on_fragment(http_parser* parser, c_char* fragment_start, size_t fragment_length)
{
    return PARSER_CONTINUE;
}

static inline int
http_on_url(http_parser* parser, c_char* url_start, size_t url_length)
{
    return PARSER_CONTINUE;
}

static inline int
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
#undef ENV_SETITEM
#undef STOP_PARSER
