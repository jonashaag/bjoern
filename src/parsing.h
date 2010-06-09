#define PARSER_OK   0
#define PARSER_CONTINUE PARSER_OK
#define PARSER_EXIT 1

http_parser_settings parser_settings;

struct _bjoern_http_parser {
    http_parser     http_parser;
    Transaction*    transaction;
    unsigned int    exit_code;

    /* Temporary variables: */
    const char*     header_name_start;
    size_t          header_name_length;
    const char*     header_value_start;
    size_t          header_value_length;
};
