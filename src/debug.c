#include "debug.h"

#ifdef SHNET_DEBUG
#include <time.h>
#include <stdio.h>
#include <stdlib.h>

static char SHNET_DEBUG_RAN = 0;

void printf_debug(const char* fmt, const int console_log, ...) {
  FILE* a = fopen("logs.txt", "a");
  if(a != NULL) {
    if(SHNET_DEBUG_RAN == 0) {
      SHNET_DEBUG_RAN = 1;
      fputs("\n", a);
    }
    {
      struct timespec tp = { .tv_sec = 0, .tv_nsec = 0 };
      (void) clock_gettime(CLOCK_REALTIME, &tp);
      fprintf(a, "[%ld] ", tp.tv_sec * 1000000000 + tp.tv_nsec);
    }
    {
      va_list args;
      va_start(args, console_log);
      vfprintf(a, fmt, args);
      fputc('\n', a);
      va_end(args);
    }
    fclose(a);
    if(console_log) {
      va_list args2;
      va_start(args2, console_log);
      vprintf(fmt, args2);
      va_end(args2);
      putc('\n', stdout);
    }
  }
}
#else
void printf_debug(const char* fmt, const int console_log, ...) {
  (void) fmt;
  (void) console_log;
}
#endif