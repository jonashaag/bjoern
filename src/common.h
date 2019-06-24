#ifndef __common_h__
#define __common_h__

#define PY_SSIZE_T_CLEAN

#include <Python.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdbool.h>
#include <string.h>

#define TYPE_ERROR_INNER(what, expected, ...) \
  PyErr_Format(PyExc_TypeError, what " must be " expected " " __VA_ARGS__)
#define TYPE_ERROR(what, expected, got) \
  TYPE_ERROR_INNER(what, expected, "(got '%.200s' object instead)", Py_TYPE(got)->tp_name)

typedef struct {
    char *data;
    size_t len;
} string;

void _init_common(void);

char *concat_str(const char *s1, const char *s2);

PyObject *_REMOTE_ADDR, *_PATH_INFO, *_QUERY_STRING, *_REQUEST_METHOD, *_GET,
        *_HTTP_CONTENT_LENGTH, *_CONTENT_LENGTH, *_HTTP_CONTENT_TYPE,
        *_CONTENT_TYPE, *_SERVER_PROTOCOL, *_SERVER_NAME, *_SERVER_PORT,
        *_http, *_HTTP_, *_HTTP_1_1, *_HTTP_1_0, *_wsgi_input, *_close,
        *_empty_string, *_empty_bytes, *_BytesIO, *_write, *_read, *_seek;

#ifdef DEBUG
#define DBG_REQ(request, ...) \
    do { \
      DBG(__VA_ARGS__); \
    } while(0)
#define DBG(...) \
    do { \
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

typedef struct {
    size_t size;
    char * buffer;
} _Buffer;

// Expandable BUFFER for io input
#define BUFFER_INIT(data) \
    do { \
        _Buffer *buf = malloc(sizeof(_Buffer)); \
        if (buf != NULL)  { \
            buf->size = 0; \
            buf->buffer = NULL; \
            data = buf; \
        } else { \
            data = NULL; \
        } \
     } while(0)
#define BUFFER_FREE(buffer) \
    do { \
        if (buffer != NULL) { \
            free(buffer); \
            buffer = NULL; \
       } \
    } while(0)
#define BUFFER_PUSH(data, value, value_len) \
    do { \
       data->size += value_len; \
       data->buffer = (char *) realloc(data->buffer, data->size); \
       if (data->buffer != NULL)  { \
          memcpy(data->buffer + data->size - value_len, value, value_len); \
       } else { \
          BUFFER_FREE(data); \
       } \
    } while(0)

#endif
