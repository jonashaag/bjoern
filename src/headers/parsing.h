/* http_parser return value codes */
#define RESPONSE_IS_CACHED         -1
#define PARSER_OK                   0
#define HTTP_NOT_FOUND              404
#define HTTP_INTERNAL_SERVER_ERROR  500
#define HTTP_NOT_IMPLEMENTED        501

struct _bjoern_http_parser {
    http_parser     http_parser;
    Transaction*    transaction;

    /* Temporary variables: */
    const char*     header_name_start;
    size_t          header_name_length;
    const char*     header_value_start;
    size_t          header_value_length;
};
