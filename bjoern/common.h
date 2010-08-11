#ifndef __common_h__
#define __common_h__

#include <Python.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdbool.h>
#include <string.h>

#define LENGTH(array) (sizeof(array)/sizeof(array[0]))
#define ADDR_FROM_MEMBER(ptr, strct, mem) (strct*)((size_t)ptr - (size_t)(&(((strct*)NULL)->mem)));

typedef enum {
    HTTP_BAD_REQUEST = 400,
    HTTP_SERVER_ERROR = 500
} http_status;

#endif
