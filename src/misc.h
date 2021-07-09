#ifndef MK_lQMNj8t_DwM9BU_vsZj_kvePzBj_d
#define MK_lQMNj8t_DwM9BU_vsZj_kvePzBj_d 1

#include <pthread.h>

extern int do_async(void* (*)(void*), void* const);

extern int do_joinable_async(void* (*)(void*), void* const, pthread_t* const);

#endif // MK_lQMNj8t_DwM9BU_vsZj_kvePzBj_d