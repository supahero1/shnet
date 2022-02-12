#ifndef tN_a_OVDejTvT_qZjNU51_kyi3Ym__CB
#define tN_a_OVDejTvT_qZjNU51_kyi3Ym__CB 1

#include <stdint.h>

enum heap_consts {  
  heap_min = -1,
  heap_max = 1
};

struct heap {
  uint64_t size;
  uint64_t used;
  char* array;
  int (*compare)(const void*, const void*);
  int sign;
  uint32_t item_size;
};

extern int   heap_resize(struct heap* const, const uint64_t);

extern int   heap_insert(struct heap* const, const void* const);

extern void* heap_pop(struct heap* const);

extern void  heap_pop_(struct heap* const);

extern void  heap_down(const struct heap* const, const uint64_t);

extern void  heap_up(const struct heap* const, const uint64_t);

extern void  heap_free(struct heap* const);

extern int   heap_is_empty(const struct heap* const);

extern void* heap_peak(const struct heap* const, const uint64_t);

extern void* heap_peak_rel(const struct heap* const, const uint64_t);

extern uint64_t heap_abs_idx(const struct heap* const, const void* const);

#endif // tN_a_OVDejTvT_qZjNU51_kyi3Ym__CB