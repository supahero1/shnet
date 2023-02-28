#ifndef _shnet_error_h_
#define _shnet_error_h_ 1

#ifdef __cplusplus
extern "C" {
#endif


__attribute__((weak))
extern int
error_handler(int error, int count);


/*
 * void* ptr = malloc(size);
 * =============== S A F E   V E R S I O N ===============
 * void* ptr;
 * safe_execute(ptr = malloc(size), ptr == NULL, ENOMEM);
 */
#define safe_execute(expression, error_condition, error)                \
do                                                                      \
{                                                                       \
    int error_counter = 0;                                              \
                                                                        \
    while(1)                                                            \
    {                                                                   \
        expression;                                                     \
                                                                        \
        if((error_condition) && !error_handler(error, error_counter))   \
        {                                                               \
            ++error_counter;                                            \
            continue;                                                   \
        }                                                               \
                                                                        \
        break;                                                          \
    }                                                                   \
}                                                                       \
while(0)


#define shnet_fallthrough() __attribute__((fallthrough))


#include <stddef.h>


extern void*
shnet_malloc(size_t size);


extern void*
shnet_calloc(size_t num, size_t size);


extern void*
shnet_realloc(void* ptr, size_t new_size);


#ifdef __cplusplus
}
#endif

#endif /* _shnet_error_h_ */
