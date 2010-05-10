typedef enum {
    RESPONSE_FINISHED = 1,
    RESPONSE_NOT_YET_FINISHED = 2,
    RESPONSE_ERROR = 3
} response_status;

typedef bool (*handler_initialize) (Transaction*);
typedef response_status (*handler_write) (Transaction*);
typedef void (*handler_finalize) (Transaction*);

typedef struct {
    handler_write writer;
    handler_finalize finalizer;
    union {
        struct wsgi_handler_data;
        struct raw_handler_data;
        struct sendfile_handler_data;
#ifdef WANT_CACHING
        struct cache_handler_data;
#endif
    };
} request_handler_data;
