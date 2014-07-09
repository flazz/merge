#include <stdio.h>
#include <stdlib.h>
#include <sys/select.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>

#include "debug.h"
#include "bitset.h"

/*
 * take all file descriptors given on cmdline
 * ignores fd 0 1 2
 * consume a line from each
 * write to stdout consumed lines sorted
 *
 */

#define MAX_INPUTS 0xf
#define MAX_RECORD_WITDH 0xff

/* select context */
int fdns;
fd_set inputs;

/* input buffers */
BITSET enabled_input_fds;
char in_buffer[MAX_INPUTS][MAX_RECORD_WITDH];
int in_buffer_size[MAX_INPUTS];

/* output buffers */
int out_buffer_count = 0;
char out_buffer[MAX_INPUTS][MAX_RECORD_WITDH];
int out_buffer_size[MAX_INPUTS];

void
read_data()
{
  for(int fd=0; fd<MAX_INPUTS; fd++) {
    if (BS_TEST(enabled_input_fds, fd) && FD_ISSET(fd, &inputs)) {
      int len = read(fd, in_buffer[fd], MAX_RECORD_WITDH);
      in_buffer_size[fd] = len;

      switch (len) {
        case 0:
          BS_UNSET(enabled_input_fds, fd);
          close(fd);
          trace("fd %d: EOF", fd);
          break;

        default:
          trace("fd %d: %d - %s", fd, in_buffer_size[fd], in_buffer[fd]);
          break;
      }

    }
  }
}

int
buffercmp(const void *s1, const void *s2)
{
  return memcmp(s1, s2, MAX_RECORD_WITDH);
}

void
transfer_buffers()
{
  for(int fd=0; fd<MAX_INPUTS; fd++) {
    if (BS_TEST(enabled_input_fds, fd) && in_buffer_size[fd]) {
      memset(out_buffer[out_buffer_count], 0, MAX_RECORD_WITDH);
      memcpy(out_buffer[out_buffer_count], in_buffer[fd], in_buffer_size[fd]);
      out_buffer_size[out_buffer_count] = in_buffer_size[fd];

      in_buffer_size[fd] = 0;
      out_buffer_count++;
    }
  }
  qsort(out_buffer, out_buffer_count, MAX_RECORD_WITDH, &buffercmp);
}

void
write_data()
{
  for(int i=0; i < out_buffer_count; i++) {
    write(1, out_buffer[i], out_buffer_size[i]);
  }
  out_buffer_count = 0;
}

int
prepare_input()
{
  int high_fd = -1;
  FD_ZERO(&inputs);

  for(int fd=0; fd<MAX_INPUTS; fd++) {

    if (BS_TEST(enabled_input_fds, fd)) {
      FD_SET(fd, &inputs);
      high_fd = (high_fd < fd) ? fd : high_fd;
    }
  }

  fdns = high_fd + 1;
  return 0;
}


void
show_select_error()
{

  switch (errno) {
    case EAGAIN:
      trace(
          "The kernel was (perhaps temporarily) unable to allocate the requested"
          "number of file descriptors."
          );
      break;

    case EBADF:
      trace(
          "One of the descriptor sets specified an invalid descriptor."
          );
      break;

    case EINTR:
      trace(
          "A signal was delivered before the time limit expired and before any of"
          "the selected events occurred."
          );
      break;

    case EINVAL:
      trace(
          "The specified time limit is invalid.  One of its components is negative"
          "or too large."
          );

      trace(
          "ndfs is greater than FD_SETSIZE and _DARWIN_UNLIMITED_SELECT is not"
          "defined."
          "nfds: %d", fdns
          );
      break;

    default:
      trace("crap out");
      break;
  }
}

void
wait_for_data() {
  prepare_input();
  int rs = select(fdns, &inputs, NULL, NULL, NULL);

  if (rs < 0) {
    show_select_error();
    exit(errno);
  } else if (rs == 0) {
    trace("should never happen");
    exit(1);
  }
}



int
main(int argc, char* argv[])
{
  BS_CLEAR(enabled_input_fds);

  for(int i = 1; i < argc; i++) {
    int fd = atoi(argv[i]);

    if (fd > 2) {
      BS_SET(enabled_input_fds, fd);
      trace("listen: %d", fd);
    }

  }

  while(enabled_input_fds) {
    wait_for_data();
    read_data();
    transfer_buffers();
    write_data();
  }

  return 0;
}
