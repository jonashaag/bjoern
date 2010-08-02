static inline void http_to_wsgi_header(char* destination, const char* source, size_t length);
static bool validate_header_tuple(PyObject*);
static void strreverse(char* begin, char* end);
static size_t uitoa10(uint32_t value, char* str);

#define OFFSETOF(mbr_name, ptr, type)  ((type*) (((char*)ptr) - offsetof(type, mbr_name)))
#define GIL_LOCK() PyGILState_STATE GILState = PyGILState_Ensure()
#define GIL_UNLOCK() PyGILState_Release(GILState)
