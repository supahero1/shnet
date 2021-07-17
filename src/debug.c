#include "debug.h"

#include <time.h>
#include <stdio.h>
#include <errno.h>
#include <stdint.h>
#include <stdlib.h>

static FILE* log_file;
static uint64_t start_time;

void _debug(const char* fmt, const int console_log, ...) {
  if(log_file == NULL) {
    log_file = fopen("logs.txt", "a");
    if(log_file == NULL) {
      return;
    }
    fprintf(log_file, "\nNew session started\n");
    struct timespec tp = { .tv_sec = 0, .tv_nsec = 0 };
    (void) clock_gettime(CLOCK_REALTIME, &tp);
    start_time = tp.tv_sec * 1000000000 + tp.tv_nsec;
  }
  struct timespec tp = { .tv_sec = 0, .tv_nsec = 0 };
  (void) clock_gettime(CLOCK_REALTIME, &tp);
  const uint64_t t = tp.tv_sec * 1000000000 + tp.tv_nsec - start_time;
  const uint64_t secs = t / 1000000000;
  fprintf(log_file, "[%4lud %4luh %4lus %5lums %5luus %5luns] ", secs / 86400, (secs / 3600) % 24, secs % 3600, (t / 1000000) % 1000, (t / 1000) % 1000, t % 1000);
  va_list args;
  va_start(args, console_log);
  vfprintf(log_file, fmt, args);
  fputc('\n', log_file);
  va_end(args);
  fflush(log_file);
  if(console_log) {
    va_list args2;
    va_start(args2, console_log);
    vprintf(fmt, args2);
    va_end(args2);
    putc('\n', stdout);
  }
}

#ifdef SHNET_DEBUG
void debug(const char* fmt, const int console_log, ...) {
  return _debug(fmt, console_log);
}
#else
void debug(const char* fmt, const int console_log, ...) {
  (void) fmt;
  (void) console_log;
}
#endif