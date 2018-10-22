#include <stdlib.h>

#include "portable_sendfile.h"

#define SENDFILE_CHUNK_SIZE 16*1024

#if defined __APPLE__

  /* OS X */

  #include <sys/socket.h>
  #include <sys/types.h>

  Py_ssize_t portable_sendfile(int out_fd, int in_fd, off_t offset) {
    off_t len = SENDFILE_CHUNK_SIZE;
    if(sendfile(in_fd, out_fd, offset, &len, NULL, 0) == -1) {
      if((errno == EAGAIN || errno == EINTR) && len > 0) {
        return len;
      }
      return -1;
    }
    return len;
  }

#elif defined(__FreeBSD__) || defined(__DragonFly__)

  #include <sys/socket.h>
  #include <sys/types.h>

  Py_ssize_t portable_sendfile(int out_fd, int in_fd, off_t offset) {
    off_t len;
    if(sendfile(in_fd, out_fd, offset, SENDFILE_CHUNK_SIZE, NULL, &len, 0) == -1) {
      if((errno == EAGAIN || errno == EINTR) && len > 0) {
        return len;
      }
      return -1;
    }
    return len;
  }

#else

  /* Linux */

  #include <sys/sendfile.h>

  Py_ssize_t portable_sendfile(int out_fd, int in_fd, off_t offset) {
    return sendfile(out_fd, in_fd, &offset, SENDFILE_CHUNK_SIZE);
  }

#endif
