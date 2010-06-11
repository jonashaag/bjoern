static bool wsgi_call_app(Transaction*);
#ifdef WANT_SENDFILE
static bool wsgi_sendfile_init(Transaction*, PyFileObject*);
static response_status wsgi_sendfile(Transaction*);
#else
static bool wsgi_mmap_init(Transaction*, PyFileObject*);
static response_status wsgi_mmap_send(Transaction*);
#endif
static response_status wsgi_send_body(Transaction*);
static void wsgi_send_headers(Transaction*);
static void wsgi_finalize(Transaction*);
