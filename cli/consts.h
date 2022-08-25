#ifndef _shnet_cli_consts_h_
#define _shnet_cli_consts_h_ 1

#include <stdint.h>
#include <stdarg.h>

struct options {
  int32_t num;
  uint32_t fast:1;
  uint32_t force:1;
};

extern struct options options;

#define DROP_IF_RIDICULOUS(num, name, min, max) \
do { \
  __typeof__ (num) _num = (num); \
  int _min = (min); \
  int _max = (max); \
  if((_num >= _max || _num <= _min) && !options.force) { \
    printf("This input doesn't seem right. It's best for the \"" name "\" option\nto be between %d and %d exclusive. " \
           "Further execution will halt.\nIf you want to bypass this warning, use the \"force\" option.\n", _min, _max); \
    return; \
  } \
} while(0)

extern void print(const char* const restrict, ...);

extern void print_time(const uint64_t);

extern void time_benchmark(void);

#endif // _shnet_cli_consts_h_
