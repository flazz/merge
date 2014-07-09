#include <stdio.h>
#include <stdlib.h>
#include <sys/select.h>
#include <unistd.h>
#include <errno.h>

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


/* long BITSET */
typedef unsigned long BITSET;
#define BS_CLEAR(set) set = 0;
#define BS_SET(set, n) set |= (1 << n)
#define BS_UNSET(set, n) set &= (0 << n)
#define BS_TEST(set, n) set & (0 << n)

/* machine state */
char buffers[MAX_INPUTS][MAX_RECORD_WITDH];
int buffer_size[MAX_INPUTS];

int enabled_input_fds[MAX_INPUTS]; /* TODO use a bit array */
/*BITSET enabled_input_fds*/

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

    if (enabled_input_fds[fd]) {
      DEBUG("prep: %d", fd);
      FD_SET(fd, &inputs);
      high_fd = (high_fd < fd) ? fd : high_fd;
    }
  }

  fdns = high_fd + 1;
  return 0;
}

int
read_data()
{
  for(int fd=0; fd<MAX_INPUTS; fd++) {
    if (enabled_input_fds[fd] && FD_ISSET(fd, &inputs)) {
      int len = read(fd, buffers[fd], MAX_RECORD_WITDH);

      switch (len) {
        case 0:
          enabled_input_fds[fd] = 0;
          close(fd);
          DEBUG("fd %d: EOF", fd);
          break;

        default:
          buffer_size[fd] = read(fd, buffers[fd], MAX_RECORD_WITDH);
          DEBUG("fd %d: %d - %s", fd, buffer_size[fd], buffers[fd]);
          break;
      }

    }
  }
  return 0;
}

int
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

  return 0;
}

int
main(int argc, char* argv[])
{
  for(int i = 0; i < MAX_INPUTS; i++)
    enabled_input_fds[i] = 0;

  for(int i = 1; i < argc; i++) {
    int fd = atoi(argv[i]);

    if (fd > 2) {
      enabled_input_fds[fd] = 1;
      DEBUG("listen: %d", fd);
    } else {
      DEBUG("skip %d", fd);
    }

  }
  DEBUG("\n")

  /*while(1) {*/
  for(int Q=0; Q<2; Q++) {
    prepare_input();
    int rs = select(fdns, &inputs, NULL, NULL, NULL);

    if (rs < 0) {
      show_select_error();
    } else if (rs == 0) {
      DEBUG("should never happen");
    } else {
      read_data();
      DEBUG("len: %lu bits", sizeof(void) * 8);
    }
  }

  DEBUG("\n")

  return 0;
}
