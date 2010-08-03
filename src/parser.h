#ifndef __parser_h__
#define __parser_h__

#include "bjoern.h"
#include "http_parser.h"
#include "wsgienv.h"
#include "request.h"

#define PARSER_OK   0
#define PARSER_CONTINUE PARSER_OK
#define PARSER_EXIT 1

typedef struct _Parser {
    http_parser parser;
    struct _Request* request;
    c_char* header_name_start;
    size_t header_name_length;
    c_char* header_value_start;
    size_t header_value_length;
} Parser;

Parser* Parser_new();
void Parser_execute(Parser*, c_char*, c_size_t);

#endif
