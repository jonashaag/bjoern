#include "common.h"

#define FileWrapper_CheckExact(x) ((x)->ob_type == &FileWrapper_Type)

PyTypeObject FileWrapper_Type;

typedef struct {
  PyObject_HEAD
  PyObject* file;
  PyObject *blocksize;
  int fd;
} FileWrapper;

void _init_filewrapper(void);
int FileWrapper_GetFd(PyObject *self);
