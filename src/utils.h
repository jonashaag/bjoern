static inline void bjoern_strcpy(char** destination, const char* source);
static inline void bjoern_http_to_wsgi_header(char* destination, const char* source, size_t length);
static PyObject* PyTuple_Pack_NoINCREF(Py_ssize_t n, ...);

#define GIL_LOCK(var) var = PyGILState_Ensure()
#define GIL_UNLOCK(var) PyGILState_Release(var)
