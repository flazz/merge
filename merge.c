#include <stdio.h>
#include <stdlib.h>
#include <sys/select.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>

/*
 * take all file descriptors given on cmdline
 * ignores fd 0 1 2
 * consume a line from each
 * write to stdout consumed lines sorted
 *
 * try them all via fcntl(fd, F_GETFD)?
 * http://stackoverflow.com/questions/12340695/how-to-check-if-a-given-file-descriptor-stored-in-a-variable-is-still-valid
 */

#define DEBUG(format, ...) \
  fprintf(stderr, "DEBUG %s,%d: ", __FILE__, __LINE__); \
  fprintf(stderr, format, ##__VA_ARGS__); \
  fprintf(stderr, "\n"); \

#define MAX_INPUTS 0xf
#define MAX_RECORD_WITDH 0xff

typedef unsigned long BITSET;
#define BS_CLEAR(set) set = 0;
#define BS_SET(set, n) set |= (1 << n)
#define BS_UNSET(set, n) set &= (0 << n)
#define BS_TEST(set, n) set & (1 << n)

/* machine state */
BITSET enabled_input_fds;
char in_buffer[MAX_INPUTS][MAX_RECORD_WITDH];
int in_buffer_size[MAX_INPUTS];

int out_buffer_count = 0;
char out_buffer[MAX_INPUTS][MAX_RECORD_WITDH];
int out_buffer_size[MAX_INPUTS];

/* global select context */
int fdns;
fd_set inputs;

/*struct timeval timeout;*/
/*timeout.tv_sec = 20;*/
/*timeout.tv_usec = 0;*/

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
          DEBUG("fd %d: EOF", fd);
          break;

        default:
          DEBUG("fd %d: %d - %s", fd, in_buffer_size[fd], in_buffer[fd]);
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
}

void
show_select_error()
{

  switch (errno) {
    case EAGAIN:
      DEBUG(
          "The kernel was (perhaps temporarily) unable to allocate the requested"
          "number of file descriptors."
          );
      break;

    case EBADF:
      DEBUG(
          "One of the descriptor sets specified an invalid descriptor."
          );
      break;

    case EINTR:
      DEBUG(
          "A signal was delivered before the time limit expired and before any of"
          "the selected events occurred."
          );
      break;

    case EINVAL:
      DEBUG(
          "The specified time limit is invalid.  One of its components is negative"
          "or too large."
          );

      DEBUG(
          "ndfs is greater than FD_SETSIZE and _DARWIN_UNLIMITED_SELECT is not"
          "defined."
          "nfds: %d", fdns
          );
      break;

    default:
      DEBUG("crap out");
      break;
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
      DEBUG("listen: %d", fd);
    }

  }

  while(enabled_input_fds) {
    prepare_input();
    int rs = select(fdns, &inputs, NULL, NULL, NULL);

    if (rs < 0) {
      show_select_error();
      exit(1);
    } else if (rs == 0) {
      DEBUG("should never happen");
      exit(2);
    } else {
      read_data();
      transfer_buffers();
      write_data();
    }

  }

  return 0;
}
