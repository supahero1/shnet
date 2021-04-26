#include "debug.h"

#ifdef SHNET_DEBUG
#include <time.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>

static char SHNET_DEBUG_RAN = 0;
#endif

void print_debug(const char* fmt, ...) {
#ifdef SHNET_DEBUG
  FILE* a = fopen("logs.txt", "a");
  if(a != NULL) {
    if(SHNET_DEBUG_RAN == 0) {
      SHNET_DEBUG_RAN = 1;
      if(fputs("========== NEW DEBUG SESSION ==========\n", a) == EOF) {
        goto error;
      }
    }
    struct timespec tp = { .tv_sec = 0, .tv_nsec = 0 };
    (void) clock_gettime(CLOCK_REALTIME, &tp);
    va_list args;
    va_start(args, fmt);
    if(fprintf(a, "[%ld] ", tp.tv_sec * 1000000000 + tp.tv_nsec) < 0 || vfprintf(a, fmt, args) < 0 || fputc('\n', a) == EOF) {
      goto error;
    }
    va_end(args);
    int b;
    do {
      b = fclose(a);
    } while(b == EINTR);
    if(b != 0 || vprintf(fmt, args) < 0) {
      goto error;
    }
  } else {
    goto error;
  }
  return;
  error:
#ifndef SHNET_NO_CONSOLE_LOG
  (void) printf("Debug error (LINE %d)", __LINE__);
#endif
  exit(1);
#endif
}