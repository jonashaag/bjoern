#ifdef DEBUG_ON
  #define DEBUG(s, ...)     fprintf(stdout, s"\n", ## __VA_ARGS__ ); fflush(stdout)
#else
  #define DEBUG(...)
#endif
#define ERROR(s, ...)       fprintf(stderr, s"\n", ## __VA_ARGS__ ); fflush(stderr)


#define EV_LOOP struct ev_loop*
#define TRANSACTION struct Transaction
#define PARSER   struct http_parser
#define BJPARSER struct bj_http_parser
#define TRANSACTION_FROM_PARSER(parser) ((BJPARSER*)parser)->transaction

#define PyString(s)             PyString_FromString(s)
#define PyStringWithLen(s, l)   PyString_FromStringAndSize(s, l)

#define FORWARD_CURSOR(p,c) p += c
#define ENSURE(ret, cond)   if(!(cond)) return ret
#define VENSURE(X)          ENSURE(, X)
#define NENSURE(X)          ENSURE(NULL, X)
#define MAX(a, b)           ((a) > (b) ? (a) : (b))
#define OFFSETOF(m, ptr, t) ((t*) (((char*)ptr) - offsetof(t, m))) /* TODO: Rename. */

#define HAVE_FLAG(X, flag)  (X & flag)
#define Have_FLAGS(X, a, b) (X & a & b)
#define SET_FLAG(X, flag)   (X |= flag)
#define UNSET_FLAG(x, flag) (X &~ flag)
