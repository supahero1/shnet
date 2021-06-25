#ifndef MK_lQMNj8t_DwM9BU_vsZj_kvePzBj_d
#define MK_lQMNj8t_DwM9BU_vsZj_kvePzBj_d 1

#include <pthread.h>

/*
 *  C O N T I N U O U S   M E M O R Y
 */

enum contmem_consts {
  contmem_success,
  contmem_out_of_memory
};

struct contmem_block {
  struct contmem_block* next;
  struct contmem_block* prev;
  unsigned long used;
};

struct contmem {
  struct contmem_block* tail;
  unsigned long block_size;
  unsigned long item_size;
  unsigned long tolerance;
};

extern int contmem(struct contmem* const, const unsigned long, const unsigned long, const unsigned long);

extern void* contmem_get(struct contmem* const);

extern int contmem_pop(struct contmem* const, void* const);

extern void contmem_pop_cleanup(struct contmem* const);

extern void* contmem_last(struct contmem* const);

extern void contmem_free(struct contmem* const);

/*
 *  A S Y N C
 */

extern int do_async(void* (*)(void*), void* const);

extern int do_joinable_async(void* (*)(void*), void* const, pthread_t* const);

#endif // MK_lQMNj8t_DwM9BU_vsZj_kvePzBj_d