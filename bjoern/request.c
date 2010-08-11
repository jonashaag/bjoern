#include "request.h"

#define REQUEST_PREALLOC_N 100

static Request* _Request_from_prealloc();
static Request _preallocd[REQUEST_PREALLOC_N];
static char _preallocd_used[REQUEST_PREALLOC_N];


Request* Request_new(int client_fd)
{
    Request* req = _Request_from_prealloc();
    if(req == NULL)
        req = malloc(sizeof(Request));
    if(req == NULL)
        return NULL;
    req->client_fd = client_fd;
    return req;
}

bool Request_parse(Request* request,
                   const char* data,
                   const size_t data_len) {
    return false;
}

void Request_write_response(Request* request,
                            const char* data,
                            size_t data_len) {
    if(!data_len)
        data_len = strlen(data);

    ssize_t sent;
    while(data_len) {
        sent = write(request->client_fd, data, data_len);
        if(sent == -1)
            return;
        data += sent;
        data_len -= sent;
    }
}

void Request_free(Request* req)
{
    if(req >= _preallocd &&
       req <= _preallocd+REQUEST_PREALLOC_N*sizeof(Request)) {
        int index = (req - _preallocd)/sizeof(Request);
        _preallocd_used[index] = false;
    } else {
        free(req);
    }
}

Request* _Request_from_prealloc()
{
    static int i = -1;

    if(i < 0) {
        memset(_preallocd_used, 0, sizeof(char)*REQUEST_PREALLOC_N);
        i = 0;
    }

    for(; i<REQUEST_PREALLOC_N; ++i) {
        if(!_preallocd_used[i]) {
            _preallocd_used[i] = true;
            return &_preallocd[i];
        }
    }
    i = 0;
    return NULL;
}
