#ifndef hZgotNx_NDps_9BZatasI3s_D_0k_KCF
#define hZgotNx_NDps_9BZatasI3s_D_0k_KCF 1

typedef int (*error_callback_t)(int);

/* This function must be set by the application before invoking any library
function. Returns 0 on success (can continue), any other value on failure (stop
execution), or exits the program on a fatal error that can't be resolved.
The function MUST NOT change errno, or at least reset it to it's value before
the function call on exit. */
extern error_callback_t handle_error;


/*
  void* ptr = malloc(size);
  =============== S A F E   V E R S I O N ===============
  void* ptr;
  safe_execute(ptr = malloc(size), ptr == NULL, ENOMEM);
*/
#define safe_execute(what, error_condition, error) \
do { \
  what; \
  if((error_condition) && handle_error(error) == 0) { \
    continue; \
  } \
} while(0)

#endif // hZgotNx_NDps_9BZatasI3s_D_0k_KCF