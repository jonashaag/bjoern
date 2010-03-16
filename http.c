#include <string.h>

int http_get_content_length(char* http_request)
{
    void* throwaway = NULL;

    char* content_length_position = strstr(http_request, "Content-Length: ");
    if(!content_length_position) return 0;

    return strtol(content_length_position + 16 /* strlen("Content-Length: ") */,
                  throwaway, 10);
}



/* Returns the position *after* the first \r\n\r\n in `c` or 0.
 * Original author: Felix von Leitner, http://ptrace.fefe.de/boyermoore.c.txt
 */
const char* find_header_end(const char* c, size_t l) {
    size_t i;
    for (i=0; i+1<l; i+=2) {
        if (c[i+1]=='\n') {
            if (c[i]=='\n') return c+i+2;
            else if (c[i]=='\r' && i+3<l && c[i+2]=='\r' && c[i+3]=='\n')
                return c+i+4;
            --i;
        } else if (c[i+1]=='\r') {
            if (i+4<l && c[i+2]=='\n' && c[i+3]=='\r' && c[i+4]=='\n')
                return c+i+5;
            --i;
        }
    }
    return 0;
}

int http_split_body(char* http_request, const size_t request_length, const char* target_body)
{
    target_body = find_header_end(http_request, request_length);
    if(!target_body)
        return 0;

    int body_length = http_request + request_length - target_body;

    /* Override the '\r\n\r\n' with '   \0'. */
    http_request[(int)body_length+0] = ' ';
    http_request[(int)body_length+1] = ' ';
    http_request[(int)body_length+2] = ' ';
    http_request[(int)body_length+3] = '\0';

    return body_length;
}
