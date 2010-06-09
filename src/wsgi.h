static bool wsgi_call_app(Transaction*);
static bool wsgi_sendfile_init(Transaction*, PyFileObject*);
static response_status wsgi_sendfile(Transaction*);
static response_status wsgi_send_body(Transaction*);
static void wsgi_send_headers(Transaction*);
static void wsgi_finalize(Transaction*);
