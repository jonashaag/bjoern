#include <stdlib.h>

#define SENDFILE_CHUNK_SIZE 16*1024

#if defined __APPLE__

  /* OS X */

  #include <sys/socket.h>
  #include <sys/types.h>

  ssize_t portable_sendfile(int out_fd, int in_fd) {
    off_t len = SENDFILE_CHUNK_SIZE;
    if(sendfile(in_fd, out_fd, 0, &len, NULL, 0) == -1)
      return -1;
    return len;
  }

#elif defined __FreeBSD__

  /* FreeBSD */

  #include <sys/socket.h>
  #include <sys/types.h>

  ssize_t portable_sendfile(int out_fd, int in_fd) {
    off_t len;
    if (sendfile(in_fd, out_fd, 0, SENDFILE_CHUNK_SIZE, NULL, &len, 0) == -1) {
      return -1;
    }
    return len;
  }

#else

  /* Linux */

  #include <sys/sendfile.h>

  ssize_t portable_sendfile(int out_fd, int in_fd) {
    return sendfile(out_fd, in_fd, NULL, SENDFILE_CHUNK_SIZE);
  }

#endif
