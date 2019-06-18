#include <stdlib.h>
#include "common.h"
#include "py3.h"

#define UNHEX(c) ((c >= '0' && c <= '9') ? (c - '0') : \
                  (c >= 'a' && c <= 'f') ? (c - 'a' + 10) : \
                  (c >= 'A' && c <= 'F') ? (c - 'A' + 10) : NOHEX)
#define NOHEX ((char) -1)

void _init_common() {

#define _(name) _##name = _PEP3333_String_FromUTF8String(#name)
    _(REMOTE_ADDR);
    _(PATH_INFO);
    _(QUERY_STRING);
    _(close);

    _(REQUEST_METHOD);
    _(SERVER_PROTOCOL);
    _(SERVER_NAME);
    _(SERVER_PORT);
    _(GET);
    _(HTTP_CONTENT_LENGTH);
    _(CONTENT_LENGTH);
    _(HTTP_CONTENT_TYPE);
    _(CONTENT_TYPE);
    _(HTTP_);
    _(http);

    _(BytesIO);
    _(write);
    _(read);
    _(seek);
#undef _

    _HTTP_1_1 = _PEP3333_String_FromUTF8String("HTTP/1.1");
    _HTTP_1_0 = _PEP3333_String_FromUTF8String("HTTP/1.0");
    _wsgi_input = _PEP3333_String_FromUTF8String("wsgi.input");
    _empty_string = _PEP3333_String_FromUTF8String("");
    _empty_bytes = _PEP3333_Bytes_FromString("");
}


char *concat_str(const char *s1, const char *s2) {
    const size_t len1 = strlen(s1);
    const size_t len2 = strlen(s2);
    char *result = malloc(len1 + len2 + 1);
    if (result == NULL)
        return NULL;  // No more juicy memory?
    memcpy(result, s1, len1);
    memcpy(result + len1, s2, len2 + 1);
    return result;
}