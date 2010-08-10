#ifndef __debug_h__
#define __debug_h__

#ifdef DEBUG
  #undef DEBUG
  #define DEBUG(s, ...)         fprintf(stdout, s"\n", ## __VA_ARGS__ )
  #define DEBUG_LINE()          DEBUG("%s:%d", __FUNCTION__, __LINE__)
  #define IF_DEBUG(statement)   statement
  #define DEBUG_REFCOUNT(obj)   DEBUG(#obj " refcnt: %d (ptr: %d)", (obj)->ob_refcnt, (obj))
#else
  #define __NOP__               do{}while(0)
  #define DEBUG(...)            __NOP__
  #define DEBUG_LINE()          __NOP__
  #define IF_DEBUG(statement)   __NOP__
  #define DEBUG_REFCOUNT(obj)   __NOP__
#endif
#define ERROR(s, ...)           fprintf(stderr, s "\n", ## __VA_ARGS__ )

#endif
