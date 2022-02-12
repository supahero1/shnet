#ifndef hZgotNx_NDps_9BZatasI3s_D_0k_KCF
#define hZgotNx_NDps_9BZatasI3s_D_0k_KCF 1

extern int error_handler(int);

/*
  void* ptr = malloc(size);
  =============== S A F E   V E R S I O N ===============
  void* ptr;
  safe_execute(ptr = malloc(size), ptr == NULL, ENOMEM);
*/
#define safe_execute(expression, error_condition, error) \
do { \
  expression; \
  if((error_condition) && !error_handler(error)) { \
    continue; \
  } \
  break; \
} while(1)

#endif // hZgotNx_NDps_9BZatasI3s_D_0k_KCF