/*
  Copyright (c) 2021 shädam

  Permission is hereby granted, free of charge, to any person obtaining a copy
  of this software and associated documentation files (the "Software"), to deal
  in the Software without restriction, including without limitation the rights
  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
  copies of the Software, and to permit persons to whom the Software is
  furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included in all
  copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
  SOFTWARE.
*/

#ifndef Qj_qc3fWVV_fzcg__TsDi_R2hW_lW__t
#define Qj_qc3fWVV_fzcg__TsDi_R2hW_lW__t 1

#ifdef __cplusplus
extern "C" {
#endif

#ifdef NET_DEBUG

#include <time.h>
#include <stdio.h>
#include <errno.h>

static char NET_DEBUG_RAN = 0;

#define NET_LOG(...) \
do { \
  FILE* a = fopen("logs.txt", "a"); \
  if(a != NULL) { \
    if(NET_DEBUG_RAN == 0) { \
      NET_DEBUG_RAN = 1; \
      (void) fputs("========== NEW SESSION ==========\n", a); \
    } \
    struct timespec tp = { .tv_sec = 0, .tv_nsec = 0 }; \
    (void) clock_gettime(CLOCK_REALTIME, &tp); \
    (void) fprintf(a, "[%ld] ", tp.tv_sec * 1000000000 + tp.tv_nsec); \
    (void) fprintf(a, __VA_ARGS__); \
    (void) fputc('\n', a); \
    int b = fclose(a); \
    if(b != 0 && b != EINTR) { \
      exit(0x10000000); \
    } \
  } \
} while(0)

#else

#define NET_LOG(...)

#endif

#ifdef __cplusplus
}
#endif

#endif // Qj_qc3fWVV_fzcg__TsDi_R2hW_lW__t