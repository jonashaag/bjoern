#define DEBUG_ON
#define READ_BUFFER_SIZE                4096
#define WRITE_SIZE                      50*4096
#define MAX_HEADER_SIZE                 1<<13   /* see [1] */
#define MAX_LISTEN_QUEUE_LENGTH         1024
#define HTTP_MAX_HEADER_NAME_LENGTH     30      /* see [2] */

#define DEFAULT_RESPONSE_CONTENT_TYPE   "text/plain"

static const char* HTTP_500_MESSAGE = "HTTP 500 Internal Server Error. Try again later.";


/*
    [1]: Average value I found looking at other server's code
    [2]: Longest header name I found was "Proxy-Authentication-Info" (25 chars)
*/
