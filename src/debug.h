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