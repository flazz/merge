#define DEBUG 0
#define trace(format, ...) \
  do { if (DEBUG) { \
  fprintf(stderr, "trace %s,%d: ", __FILE__, __LINE__); \
  fprintf(stderr, format, ##__VA_ARGS__); \
  fprintf(stderr, "\n"); \
  } } while (0);
