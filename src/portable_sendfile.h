#include <sys/types.h> /* for off_t */
#include <Python.h> /* for Py_ssize_t */

Py_ssize_t portable_sendfile(int out_fd, int in_fd, off_t offset);
