#ifndef MK_lQMNj8t_DwM9BU_vsZj_kvePzBj_d
#define MK_lQMNj8t_DwM9BU_vsZj_kvePzBj_d 1

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

extern void* contmem_last(struct contmem* const);

extern void contmem_free(struct contmem* const);

/*
 *  M U T U A L   F U N C T I O N   E X C L U S I O N
 */

#include <stdint.h>
#include <pthread.h>
#include <stdatomic.h>

enum mufex_consts {
  mufex_shared,
  mufex_not_shared,
  
  mufex_success = 0,
  mufex_failure
};

struct mufex {
  pthread_mutex_t mutex;
  _Atomic uint32_t counter;
};

extern int mufex(struct mufex* const);

extern void mufex_lock(struct mufex* const, const int);

extern void mufex_unlock(struct mufex* const, const int);

extern void mufex_destroy(struct mufex* const);

/*
 *  A S Y N C
 */

extern int do_async(void* (*)(void*), void* const);

extern int do_joinable_async(void* (*)(void*), void* const, pthread_t* const);

#endif // MK_lQMNj8t_DwM9BU_vsZj_kvePzBj_d