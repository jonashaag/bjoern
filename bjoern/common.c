#include "common.h"

#define UNHEX(c) ((c >= '0' && c <= '9') ? (c - '0') : \
                  (c >= 'a' && c <= 'f') ? (c - 'a' + 10) : \
                  (c >= 'A' && c <= 'F') ? (c - 'A' + 10) : NOHEX)
#define NOHEX -1

size_t unquote_url_inplace(char* url, size_t len) {
    for(char *p=url, *end=url+len; url != end; ++url, ++p) {
        if(*url == '%') {
            if(url >= end-2) {
                /* Less than two characters left after the '%' */
                return 0;
            }
            char a = UNHEX(url[1]);
            char b = UNHEX(url[2]);
            if(a == NOHEX || b == NOHEX) return 0;
            *p = a*16 + b;
            url += 2;
            len -= 2;
        } else {
            *p = *url;
        }
    }
    return len;
}

/* Case insensitive string comparison */
bool string_iequal(const char* a, const size_t len, const char* b)
{
    if(len != strlen(b))
        return false;
    for(size_t i=0; i<len; ++i)
        if(a[i] != b[i] && a[i] - ('a'-'A') != b[i])
            return false;
    return true;
}

void _initialize_static_strings() {
    #define _(name) _##name = PyString_FromString(#name)
    _(REMOTE_ADDR); _(PATH_INFO); _(QUERY_STRING); _(REQUEST_URI);
    _(HTTP_FRAGMENT); _(REQUEST_METHOD); _(SERVER_PROTOCOL); _(GET);
    _(close); _(0); _(Connection);
    _Content_Length = PyString_FromString("Content-Length");
    _Content_Type = PyString_FromString("Content-Type");
    _HTTP_1_1 = PyString_FromString("HTTP/1.1");
    _HTTP_1_0 = PyString_FromString("HTTP/1.0");
    _wsgi_input = PyString_FromString("wsgi.input");
    _empty_string = PyString_FromString("");
    #undef _
}
