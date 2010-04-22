struct bj_http_parser {
    PARSER        http_parser;
    TRANSACTION*  transaction;

    /* Temporary variables: */
    const char*   header_name_start;
    size_t        header_name_length;
    const char*   header_value_start;
    size_t        header_value_length;
};

enum http_parser_error { HTTP_PARSER_ERROR_REQUEST_METHOD_NOT_SUPPORTED = 1 };
typedef enum http_method http_method;
