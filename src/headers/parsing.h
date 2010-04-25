struct _bjoern_http_parser {
    http_parser     http_parser;
    Transaction*    transaction;

    /* Temporary variables: */
    const char*     header_name_start;
    size_t          header_name_length;
    const char*     header_value_start;
    size_t          header_value_length;
};
