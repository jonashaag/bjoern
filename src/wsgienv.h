#include "Python.h"

#define Environ_new PyDict_New
#define Environ_SetItem PyDict_SetItem

void Environ_SetItemWithLength(PyObject* env, const char*, const size_t,
                                              const char*, const size_t);

static inline void
copy_wsgi_header(char* restrict, const char* restrict, const size_t);
