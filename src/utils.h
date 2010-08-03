#include <stdbool.h>
#include <Python.h>

bool validate_header_tuple(PyObject*);
size_t uitoa10(uint32_t value, char* str);
static void strreverse(char* begin, char* end);
