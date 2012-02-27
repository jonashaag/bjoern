#include "common.h"

#define UNHEX(c) ((c >= '0' && c <= '9') ? (c - '0') : \
                  (c >= 'a' && c <= 'f') ? (c - 'a' + 10) : \
                  (c >= 'A' && c <= 'F') ? (c - 'A' + 10) : NOHEX)
#define NOHEX -1

PyObject *_REMOTE_ADDR, *_PATH_INFO, *_QUERY_STRING, *_REQUEST_METHOD, *_GET,
         *_HTTP_CONTENT_LENGTH, *_CONTENT_LENGTH, *_HTTP_CONTENT_TYPE, *_CONTENT_TYPE,
         *_SERVER_PROTOCOL, *_HTTP_1_1, *_HTTP_1_0, *_wsgi_input, *_close, *_empty_string;

size_t unquote_url_inplace(char* url, size_t len)
{
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

void _init_common()
{
  #define _(name) _##name = PyString_FromString(#name)
  _(REMOTE_ADDR); _(PATH_INFO); _(QUERY_STRING); _(close);
  _(REQUEST_METHOD); _(SERVER_PROTOCOL); _(GET);
  _(HTTP_CONTENT_LENGTH); _(CONTENT_LENGTH); _(HTTP_CONTENT_TYPE); _(CONTENT_TYPE);
  _HTTP_1_1 = PyString_FromString("HTTP/1.1");
  _HTTP_1_0 = PyString_FromString("HTTP/1.0");
  _wsgi_input = PyString_FromString("wsgi.input");
  _empty_string = PyString_FromString("");
  #undef _
}
