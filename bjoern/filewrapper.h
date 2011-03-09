#include "common.h"

PyTypeObject FileWrapper_Type;

typedef struct {
  PyObject_HEAD
  PyObject* file;
} FileWrapper;

void (_init_filewrapper)();
