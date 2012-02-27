#include <stdlib.h>
#include <sys/types.h>
#include <sys/sendfile.h>

#define SENDFILE_CHUNK_SIZE 16*1024

#ifdef __APPLE__

  /* OS X sendfile */
  ssize_t portable_sendfile(int out_fd, int in_fd) {
    off_t len = SENDFILE_CHUNK_SIZE;
    if(sendfile(in_fd, out_fd, 0, &len, NULL, 0) == -1)
      return -1;
    return len;
  }

#else

  /* Linux sendfile */
  ssize_t portable_sendfile(int out_fd, int in_fd) {
    return sendfile(out_fd, in_fd, NULL, SENDFILE_CHUNK_SIZE);
  }

#endif
