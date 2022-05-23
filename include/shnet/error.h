#ifndef hZgotNx_NDps_9BZatasI3s_D_0k_KCF
#define hZgotNx_NDps_9BZatasI3s_D_0k_KCF 1

#ifdef __cplusplus
extern "C" {
#endif

extern int error_handler(int, int);

/*
  void* ptr = malloc(size);
  =============== S A F E   V E R S I O N ===============
  void* ptr;
  safe_execute(ptr = malloc(size), ptr == NULL, ENOMEM);
*/
#define safe_execute(expression, error_condition, error) \
do { \
  int error_counter = 0; \
  while(1) { \
    expression; \
    if((error_condition) && !error_handler(error, error_counter)) { \
      ++error_counter; \
      continue; \
    } \
    break; \
  } \
} while(0)

#include <stddef.h>

extern void* shnet_malloc(const size_t);

extern void* shnet_calloc(const size_t, const size_t);

extern void* shnet_realloc(void* const, const size_t);

#ifdef __cplusplus
}
#endif

#endif // hZgotNx_NDps_9BZatasI3s_D_0k_KCF
