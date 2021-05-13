#ifndef MK_lQMNj8t_DwM9BU_vsZj_kvePzBj_d
#define MK_lQMNj8t_DwM9BU_vsZj_kvePzBj_d 1

#ifdef __cplusplus
extern "C" {
#endif

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

int contmem(struct contmem* const, const unsigned long, const unsigned long, const unsigned long);

void* contmem_get(struct contmem* const);

int contmem_pop(struct contmem* const, void* const);

void* contmem_last(struct contmem* const);

void contmem_free(struct contmem* const);

#ifdef __cplusplus
}
#endif

#endif // MK_lQMNj8t_DwM9BU_vsZj_kvePzBj_d