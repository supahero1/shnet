#include "debug.h"

#include <time.h>
#include <stdio.h>
#include <errno.h>
#include <stdint.h>
#include <stdlib.h>
#include <pthread.h>

static FILE* debug_log_file = NULL;
static uint64_t debug_start_time;

static pthread_mutex_t debug_mutex = PTHREAD_MUTEX_INITIALIZER;

void _debug(const char* fmt, const int console_log, ...) {
  if(debug_log_file == NULL) {
    debug_log_file = fopen("logs.txt", "a");
    if(debug_log_file == NULL) {
      fprintf(stderr, "_debug(): can't open logs.txt, errno %d\n", errno);
      return;
    }
    pthread_mutex_lock(&debug_mutex);
    fprintf(debug_log_file, "\nNew session started\n");
    struct timespec tp = { .tv_sec = 0, .tv_nsec = 0 };
    (void) clock_gettime(CLOCK_REALTIME, &tp);
    debug_start_time = tp.tv_sec * 1000000000 + tp.tv_nsec;
  } else {
    pthread_mutex_lock(&debug_mutex);
  }
  struct timespec tp = { .tv_sec = 0, .tv_nsec = 0 };
  (void) clock_gettime(CLOCK_REALTIME, &tp);
  const uint64_t t = tp.tv_sec * 1000000000 + tp.tv_nsec - debug_start_time;
  const uint64_t secs = t / 1000000000; /* days, hours, minutes, seconds, milliseconds, microseconds */
  fprintf(debug_log_file, "[%2lu/%2lu/%2lu/%2lu/%3lu/%3lu] ", (secs / 86400) % 100, (secs / 3600) % 24, (secs / 60) % 60, secs % 60, (t / 1000000) % 1000, (t / 1000) % 1000);
  va_list args;
  va_start(args, console_log);
  vfprintf(debug_log_file, fmt, args);
  fputc('\n', debug_log_file);
  va_end(args);
  fflush(debug_log_file);
  if(console_log) {
    va_list args2;
    va_start(args2, console_log);
    vprintf(fmt, args2);
    va_end(args2);
    putc('\n', stdout);
  }
  pthread_mutex_unlock(&debug_mutex);
}

void debug(const char* fmt, const int console_log, ...) {
#ifdef SHNET_DEBUG
  return _debug(fmt, console_log);
#else
  (void) fmt;
  (void) console_log;
#endif
}

void debug_free() {
  if(debug_log_file != NULL) {
    (void) fclose(debug_log_file);
    debug_log_file = NULL;
  }
}