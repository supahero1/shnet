#ifndef f5TG_Z_j_0I_DXyshUC__KV_L7LW_JqD
#define f5TG_Z_j_0I_DXyshUC__KV_L7LW_JqD 1

#include <stdint.h>

struct contmem_block {
  struct contmem_block* next;
  struct contmem_block* prev;
  uint64_t used;
};

struct contmem {
  struct contmem_block* tail;
  uint64_t block_size;
  uint64_t item_size;
  uint64_t tolerance;
};

extern int contmem(struct contmem* const, const uint64_t, const uint64_t, const uint64_t);

extern void* contmem_get(struct contmem* const);

extern int contmem_pop(struct contmem* const, void* const);

extern void contmem_pop_cleanup(struct contmem* const);

extern void* contmem_last(struct contmem* const);

extern void contmem_free(struct contmem* const);

#endif // f5TG_Z_j_0I_DXyshUC__KV_L7LW_JqD