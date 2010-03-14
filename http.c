#include <string.h>

int http_get_content_length(char* http_request)
{
    void* throwaway = NULL;

    char* content_length_position = strstr(http_request, "Content-Length: ");
    if(!content_length_position) return 0;

    return strtol(content_length_position + 16 /* strlen("Content-Length: ") */,
                  throwaway, 10);
}

int http_split_body(char* http_request, const int request_length, char* target_body)
{
    target_body = strstr(http_request, "\r\n\r\n");
    if(!target_body)
        return 0;

    /* Move the body cursor forward by 4 to skip the '\r\n\r\n.' */
    target_body += 4;

    int body_length = http_request + request_length - target_body;

    /* Override the '\r\n\r\n' with '   \0'. */
    http_request[(int)body_length+0] = ' ';
    http_request[(int)body_length+1] = ' ';
    http_request[(int)body_length+2] = ' ';
    http_request[(int)body_length+3] = '\0';

    return body_length;
}
