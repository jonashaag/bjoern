#include "bjoern.h"

#define Environ_new PyDict_New
#define Environ_SetItem PyDict_SetItem

void Environ_SetItemWithLength(PyObject* env, c_char*, c_size_t, c_char*, c_size_t);
