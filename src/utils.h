static inline void bjoern_strcpy(char** destination, const char* source);
static inline void bjoern_http_to_wsgi_header(char* destination, const char* source, size_t length);

#define OFFSETOF(mbr_name, ptr, type)  ((type*) (((char*)ptr) - offsetof(type, mbr_name)))
#define GIL_LOCK() PyGILState_STATE GILState = PyGILState_Ensure()
#define GIL_UNLOCK() PyGILState_Release(GILState)
