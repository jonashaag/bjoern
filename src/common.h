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

// Expandable BUFFER for io input
#define BUFFER_CHUNK_SIZE 16 * 1024
#define BUFFER_CREATE(data) \
    do { \
        size_t *buf = malloc(3 * sizeof(size_t)); \
        if (buf != NULL)  { \
            buf[0] = 0; \
            buf[1] = 0; \
            buf[2] = 0; \
            data = (char *) &buf[3]; \
        } else { \
            data = NULL; \
        } \
} while(0)

#define BUFFER_DESTROY(data)    \
    do { \
        size_t *buf = ((size_t *) (data) - 3); \
        free(buf); \
        data = NULL; \
    } while(0)

#define BUFFER_LEN(BUFFER) (*((size_t *) BUFFER - 3))
#define BUFFER_CAPACITY(BUFFER) (*((size_t *) BUFFER - 2)) * BUFFER_CHUNK_SIZE
#define BUFFER_SIZE(BUFFER) (*((size_t *) BUFFER - 1))

#define BUFFER_PUSH(data, value)\
do { \
    fprintf(stderr, "push: %zd:%zd \n\n", BUFFER_CAPACITY(data), strlen(value)); \
    size_t *buf = ((size_t *) (data) - 3); \
    fprintf(stderr, "push: %zd:%zd:%zd \n\n", buf[0], buf[1], buf[2]); \
    buf[0] = buf[0] + 1; \
    if(buf[1] == 0) { \
         buf[1] = 1; \
         buf[2] = strlen(value); \
         fprintf(stderr, "realloc0: %zd\n", buf[2]); \
         buf = realloc(buf, 3 * sizeof(size_t) + buf[1] * BUFFER_CHUNK_SIZE); \
         if (buf != NULL)  { \
            (data) = (char *) &buf[3]; \
         } else { \
             data = NULL; \
         } \
    } \
    if(buf[0] > buf[1]) { \
         buf[1] += 1; \
         buf[2] += strlen(value); \
         fprintf(stderr, "realloc1: %zd\n", buf[2]); \
         buf = realloc(buf, 3 * sizeof(size_t) + buf[1] * BUFFER_CHUNK_SIZE); \
         if (buf == NULL) { \
            if (data != NULL) { \
               fprintf(stderr, "destroy0:\n"); \
               fprintf(stderr, "destroy0: %zd\n", BUFFER_CAPACITY(data)); \
               BUFFER_DESTROY(data); \
            } \
            data = NULL; \
         } else { \
            (data) = (char *) &buf[3]; \
         } \
     } \
    if (data != NULL && BUFFER_CAPACITY(data) < strlen(value)){ \
        fprintf(stderr, "destroy1:\n"); \
        fprintf(stderr, "destroy1: %zd\n", BUFFER_CAPACITY(data)); \
        BUFFER_DESTROY(data); \
        data = NULL; \
    } \
    if (data != NULL) \
        data[buf[0] - 1] = (*((char *)(value))); \
 } while(0)

#endif
