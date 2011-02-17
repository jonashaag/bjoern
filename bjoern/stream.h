#include "common.h"

#define STREAM_CHUNK_SIZE 4*1024 /* XXX this is kind of random */

PyTypeObject StreamType;

typedef struct {
  PyObject_HEAD
  string data;
  size_t pos;
} Stream;
