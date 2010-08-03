#ifndef __parser_h__
#define __parser_h__

#include "http_parser.h"
#include "request.h"

#define PARSER_OK   0
#define PARSER_CONTINUE PARSER_OK
#define PARSER_EXIT 1

typedef struct _Parser {
    http_parser parser;
    struct _Request* request;
    const char* header_name_start;
    size_t header_name_length;
    const char* header_value_start;
    size_t header_value_length;
} Parser;

Parser* Parser_new();
void Parser_execute(Parser*, const char*, const size_t);

#endif
