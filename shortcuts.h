#ifdef DEBUG
  #undef DEBUG
  #define DEBUG(s, ...)         fprintf(stdout, s"\n", ## __VA_ARGS__ )
  #define DEBUG_LINE()          DEBUG("%s:%d", __FUNCTION__, __LINE__)
  #define IF_DEBUG(statement)   statement
#else
  #define DEBUG(...)
  #define DEBUG_LINE()
  #define IF_DEBUG(statement)
#endif
#define ERROR(s, ...)           fprintf(stderr, s"\n", ## __VA_ARGS__ )


#define ALLOC(size)             calloc(1, size)
#define EV_LOOP                 struct ev_loop*
#define TRANSACTION             struct Transaction
#define PARSER                  struct http_parser
#define BJPARSER                struct bj_http_parser

#define PyString(s)             PyString_FromString(s)
#define PyStringWithLen(s, l)   PyString_FromStringAndSize(s, l)
#define PyB_Err(...)            (void*)PyErr_Format(__VA_ARGS__)

#define FORWARD_CURSOR(p,c)     p += c
#define MAX(a, b)               ((a) > (b) ? (a) : (b))
#define OFFSETOF(m, ptr, t)     ((t*) (((char*)ptr) - offsetof(t, m))) /* TODO: Rename. */

#define HAVE_FLAG(X, flag)      (X & flag)
#define Have_FLAGS(X, a, b)     (X & a & b)
#define SET_FLAG(X, flag)       (X |= flag)
#define UNSET_FLAG(x, flag)     (X &~ flag)
