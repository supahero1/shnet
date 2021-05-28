#ifndef y2x_B_NCq_lcbJE_FmpqVhYlnV_Ij3k5
#define y2x_B_NCq_lcbJE_FmpqVhYlnV_Ij3k5 1

#include "heap.h"

struct refheap_ref {
  void** ref;
};

extern struct heap refheap(const unsigned long, const long, long (*)(const void*, const void*), const unsigned long);

extern unsigned long refheap_ref_to_idx(const struct heap* const, const void* const);

extern int refheap_resize(struct heap* const, unsigned long);

extern int refheap_insert(struct heap* const, const void* const, void** const);

extern void refheap_pop(struct heap* const);

extern void refheap_down(const struct heap* const, const unsigned long);

extern void refheap_up(const struct heap* const, const unsigned long);

extern void refheap_delete(struct heap* const, void* const);

extern void refheap_free(struct heap* const);

extern int refheap_is_empty(const struct heap* const);

extern void* refheap_peak(const struct heap* const, const unsigned long);

#endif // y2x_B_NCq_lcbJE_FmpqVhYlnV_Ij3k5