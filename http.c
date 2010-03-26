#define HTTP_MAX_HEADER_NAME_LENGTH 30
/* Longest header name I found was "Proxy-Authentication-Info" (25 chars) */

#define GET_TRANSACTION TRANSACTION_FROM_PARSER(parser)

static int http_on_start_parsing(PARSER* parser)
{
    ((BJPARSER*)parser)->header_name_start   = NULL;
    ((BJPARSER*)parser)->header_name_length  = 0;
    ((BJPARSER*)parser)->header_value_start  = NULL;
    ((BJPARSER*)parser)->header_value_length = 0;
    return 0;
}

static int http_on_end_parsing(PARSER* parser)
{
    return 0;
}

static int http_set_path(PARSER* parser,
                         const char* path_start,
                         size_t path_length)
{
    PyObject* py_tmp = PyStringWithLen(path_start, path_length);
    Py_INCREF(py_tmp);
    GET_TRANSACTION->request_path = py_tmp;
    return 0;
}

static int http_set_query(PARSER* parser,
                          const char* query_start,
                          size_t query_length)
{
    PyObject* py_tmp = PyStringWithLen(query_start, query_length);
    Py_INCREF(py_tmp);
    GET_TRANSACTION->request_query = py_tmp;
    return 0;
}

static int http_set_url(PARSER* parser,
                        const char* url_start,
                        size_t url_length)
{
    PyObject* py_tmp = PyStringWithLen(url_start, url_length);
    Py_INCREF(py_tmp);
    GET_TRANSACTION->request_url = py_tmp;
    return 0;
}

static int http_set_fragment(PARSER* parser,
                             const char* fragment_start,
                             size_t fragment_length)
{
    PyObject* py_tmp = PyStringWithLen(fragment_start, fragment_length);
    Py_INCREF(py_tmp);
    GET_TRANSACTION->request_url_fragment = py_tmp;
    return 0;
}


static int http_set_header(PARSER* parser,
                           const char* header_start,
                           size_t header_length)
{
    TRANSACTION* transaction = GET_TRANSACTION;

    if(((BJPARSER*)parser)->header_value_start) {
        DEBUG("CPY for %d", transaction->num);
        /* We have a name/value pair to store, so do so. */
        PyObject* py_tmp1 = PyStringWithLen(
            ((BJPARSER*)parser)->header_name_start,
            ((BJPARSER*)parser)->header_name_length
        );
        PyObject* py_tmp2 = PyStringWithLen(
            ((BJPARSER*)parser)->header_value_start,
            ((BJPARSER*)parser)->header_value_length
        );
        Py_INCREF(py_tmp1);
        Py_INCREF(py_tmp2);


        PyDict_SetItem(transaction->request_headers, py_tmp1, py_tmp2);
        goto start_new_header;
    }
    if(((BJPARSER*)parser)->header_name_start) {
        /*  We already have a pointer to the header, so update the length. */
        /* TODO: Documentation */
        ((BJPARSER*)parser)->header_name_length =  (header_start - ((BJPARSER*)parser)->header_name_start)
                                     + header_length;
        return 0;
    }
    else {
        goto start_new_header;
    }

/* Start a new header. */
start_new_header:
    ((BJPARSER*)parser)->header_name_start = header_start;
    ((BJPARSER*)parser)->header_name_length = header_length;

    DEBUG("Length: %d on %d", header_length, transaction->num);

    return 0;
}

static int http_set_header_value(PARSER* parser,
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


static int http_on_headers_complete(PARSER* parser)
{
    return 0;
}

static int http_set_body(PARSER* parser,
                         const char* body,
                         size_t body_length)
{
    PyObject* py_tmp = PyStringWithLen(body, body_length);
    Py_INCREF(py_tmp);
    GET_TRANSACTION->request_body = py_tmp;
    return 0;
}
