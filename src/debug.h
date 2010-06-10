#ifdef DEBUG
  #undef DEBUG
  #define DEBUG(s, ...)         fprintf(stdout, s"\n", ## __VA_ARGS__ )
  #define DEBUG_LINE()          DEBUG("%s:%d", __FUNCTION__, __LINE__)
  #define IF_DEBUG(statement)   statement
#else
  #define DEBUG(...)            do{}while(0)
  #define DEBUG_LINE()
  #define IF_DEBUG(statement)
#endif
#define ERROR(s, ...)           fprintf(stderr, s"\n", ## __VA_ARGS__ )
