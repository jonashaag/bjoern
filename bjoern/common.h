#ifndef __common_h__
#define __common_h__

#include <Python.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdbool.h>
#include <string.h>

void _initialize_static_strings();
size_t unquote_url_inplace(char* url, size_t len);
bool string_iequal(const char* a, const size_t len, const char* b);

typedef enum {
    HTTP_BAD_REQUEST = 1, HTTP_LENGTH_REQUIRED, HTTP_SERVER_ERROR
} http_status;

PyObject *_REMOTE_ADDR, *_PATH_INFO, *_QUERY_STRING, *_REQUEST_URI,
         *_HTTP_FRAGMENT, *_REQUEST_METHOD, *_SERVER_PROTOCOL, *_GET,
         *_Content_Length, *_Content_Type, *_Connection, *_HTTP_1_1,
         *_HTTP_1_0, *_wsgi_input, *_close, *_0, *_empty_string;

#ifdef DEBUG
    #define DBG_REQ(request, ...) \
        do { \
            printf("[DEBUG Req %ld] ", request->id); \
            DBG(__VA_ARGS__); \
        } while(0)
    #define DBG(...) \
        do { \
            printf(__VA_ARGS__); \
            printf("\n"); \
        } while(0)
#else
    #define DBG(...) do{}while(0)
    #define DBG_REQ(...) DBG(__VA_ARGS__)
#endif

#define DBG_REFCOUNT(obj) \
    DBG(#obj "->obj_refcnt: %d", obj->ob_refcnt)

#define DBG_REFCOUNT_REQ(request, obj) \
    DBG_REQ(request, #obj "->ob_refcnt: %d", obj->ob_refcnt)

#ifdef WITHOUT_ASSERTS
    #undef assert
    #define assert(...) do{}while(0)
#endif

#endif
