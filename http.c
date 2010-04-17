/* Some predefined, often-required objects. */
static PyObject* PY_STRING_GET;
static PyObject* PY_STRING_POST;
static PyObject* PY_STRING_REQUEST_METHOD;
static PyObject* PY_STRING_PATH_INFO;
static PyObject* PY_STRING_QUERY_STRING;
static PyObject* PY_STRING_HTTP_CONTENT_TYPE;
static PyObject* PY_STRING_CONTENT_TYPE;
static PyObject* PY_STRING_SERVER_NAME;
static PyObject* PY_STRING_SERVER_PORT;
static PyObject* PY_STRING_SERVER_PROTOCOL;
static PyObject* PY_STRING_Content_Type;    /* "Content-Type" */
static PyObject* PY_STRING_Content_Length;  /* "Content-Length" */
static PyObject* PY_STRING_DEFAULT_RESPONSE_CONTENT_TYPE; /* DEFAULT_RESPONSE_CONTENT_TYPE */

#define GET_TRANSACTION ((BJPARSER*)parser)->transaction

/*
    Initialize the http parser.
*/
static int http_on_start_parsing(PARSER* parser)
{
    ((BJPARSER*)parser)->header_name_start   = NULL;
    ((BJPARSER*)parser)->header_name_length  = 0;
    ((BJPARSER*)parser)->header_value_start  = NULL;
    ((BJPARSER*)parser)->header_value_length = 0;
    return 0;
}

/*
    Parsing is done, populate some WSGI `environ` keys and so on.
*/
static int http_on_end_parsing(PARSER* parser)
{
    /* Set the REQUEST_METHOD: */
    PyObject* py_request_method;

    switch(parser->method) {
        case HTTP_GET:
            py_request_method = PY_STRING_GET;
            break;
        case HTTP_POST:
            py_request_method = PY_STRING_GET;
            break;
        default:
            /* Currently, only POST and GET is supported. Fail here. */
            return HTTP_PARSER_ERROR_REQUEST_METHOD_NOT_SUPPORTED;
    }

    PyDict_SetItem(
        GET_TRANSACTION->wsgi_environ,
        PY_STRING_REQUEST_METHOD,
        py_request_method
    );

    /* Set the CONTENT_TYPE, which is the same as HTTP_CONTENT_TYPE. */
    PyObject* content_type = PyDict_GetItem(GET_TRANSACTION->wsgi_environ,
                                            PY_STRING_HTTP_CONTENT_TYPE);
    if(content_type) {
        PyDict_SetItem(GET_TRANSACTION->wsgi_environ,
                       PY_STRING_CONTENT_TYPE, content_type);
    }

    /* TODO: Set SERVER_NAME and SERVER_PORT. */
    return 0;
}

/*
    Set SCRIPT_NAME and PATH_INFO.

    TODO: Implement when routing is implemented.
*/
static int http_on_path(PARSER* parser,
                        const char* path_start,
                        size_t path_length)
{
    PyObject* py_tmp = PyStringWithLen(path_start, path_length);
    Py_INCREF(py_tmp);

    PyDict_SetItem(GET_TRANSACTION->wsgi_environ, PY_STRING_PATH_INFO, py_tmp);
    return 0;
}

/*
    Set the QUERY_STRING.
*/
static int http_on_query(PARSER* parser,
                         const char* query_start,
                         size_t query_length)
{
    PyObject* py_tmp = PyStringWithLen(query_start, query_length);
    Py_INCREF(py_tmp);

    PyDict_SetItem(GET_TRANSACTION->wsgi_environ, PY_STRING_QUERY_STRING, py_tmp);
    return 0;
}


/*
    Transform the current header name to something WSGI (CGI) compatible, e.g.
        User-Agent => HTTP_USER_AGENT
    and store it in the `wsgi_environ` dictionary.
*/
static inline void store_current_header(BJPARSER* parser)
{
    /* Allocate an empty Python string with size 'header-length + 5': */
    PyObject* py_header_name = PyString_FromStringAndSize(NULL,
                                  parser->header_name_length + strlen("HTTP_"));
    Py_INCREF(py_header_name);
    /* Get the internal buffer of `py_header_name`: */
    char* header_name = PyString_AS_STRING(py_header_name);

    /* Copy the new header name: */
    header_name[0] = 'H';
    header_name[1] = 'T';
    header_name[2] = 'T';
    header_name[3] = 'P';
    header_name[4] = '_';

    bjoern_http_to_wsgi_header(&header_name[5], parser->header_name_start,
                                                parser->header_name_length);

    PyObject* py_header_value = PyStringWithLen(parser->header_value_start,
                                                parser->header_value_length);
    Py_INCREF(py_header_value);


    PyDict_SetItem(GET_TRANSACTION->wsgi_environ, py_header_name, py_header_value);
}


static int http_on_header_name(PARSER* parser,
                               const char* header_start,
                               size_t header_length)
{
    if(((BJPARSER*)parser)->header_value_start) {
        /* We have a name/value pair to store, so do so. */
        store_current_header((BJPARSER*)parser);
        goto start_new_header;
    }
    if(((BJPARSER*)parser)->header_name_start) {
        /*  We already have a pointer to the header, so update the length. */
        /* TODO: Documentation */
        ((BJPARSER*)parser)->header_name_length = \
            (header_start - ((BJPARSER*)parser)->header_name_start) + header_length;
        return 0;
    }
    else {
        goto start_new_header;
    }

/* Start a new header. */
start_new_header:
    ((BJPARSER*)parser)->header_value_start = NULL;
    ((BJPARSER*)parser)->header_value_length = 0;
    ((BJPARSER*)parser)->header_name_start = header_start;
    ((BJPARSER*)parser)->header_name_length = header_length;

    return 0;
}

static int http_on_header_value(PARSER* parser,
                                const char* value_start,
                                size_t value_length)
{
    if(((BJPARSER*)parser)->header_value_start) {
        /* We already have a value pointer, so update the length. */
        ((BJPARSER*)parser)->header_value_length =
            (value_start - ((BJPARSER*)parser)->header_value_start) + value_length;
    }
    else {
        /* Start new value. */
        ((BJPARSER*)parser)->header_value_start = value_start;
        ((BJPARSER*)parser)->header_value_length = value_length;
    }

    return 0;
}



/* TODO: Implement with StringIO or something like that. */
static int http_on_body(PARSER* parser, const char* body, size_t body_length) { return 0; }

/* TODO: Decide what to do with this one. */
static int http_on_fragment(PARSER* parser, const char* fragment_start, size_t fragment_length) { return 0; }

static  int http_on_url(PARSER* parser, const char* url_start, size_t url_length) { return 0; }
static int http_on_headers_complete(PARSER* parser) { return 0; }


static struct http_parser_settings
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
