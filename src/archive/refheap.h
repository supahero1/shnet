#ifndef y2x_B_NCq_lcbJE_FmpqVhYlnV_Ij3k5
#define y2x_B_NCq_lcbJE_FmpqVhYlnV_Ij3k5 1

#include "heap.h"

enum refheap_consts {  
  refheap_min = heap_min,
  refheap_max = heap_max
};

extern int   refheap_resize(struct heap* const, const uint64_t);

extern int   refheap_insert(struct heap* const, const void* const, uint64_t* const);

extern void* refheap_pop(struct heap* const);

extern void  refheap_pop_(struct heap* const);

extern void  refheap_down(const struct heap* const, const uint64_t);

extern void  refheap_up(const struct heap* const, const uint64_t);

extern void  refheap_delete(struct heap* const, const uint64_t);

extern void  refheap_free(struct heap* const);

extern int   refheap_is_empty(const struct heap* const);

extern void* refheap_peak(const struct heap* const, const uint64_t);

extern void* refheap_peak_rel(const struct heap* const, const uint64_t);

extern uint64_t refheap_abs_idx(const struct heap* const, const void* const);

extern void  refheap_inject(const struct heap* const, const uint64_t, uint64_t* const);

extern void  refheap_inject_rel(const struct heap* const, const uint64_t, uint64_t* const);

#endif // y2x_B_NCq_lcbJE_FmpqVhYlnV_Ij3k5
