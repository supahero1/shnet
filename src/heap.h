#ifndef tN_a_OVDejTvT_qZjNU51_kyi3Ym__CB
#define tN_a_OVDejTvT_qZjNU51_kyi3Ym__CB 1

enum heap_consts {
  heap_success,
  heap_out_of_memory,
  heap_failure,
  
  heap_min = -1,
  heap_max = 1
};

struct heap {
  char* array;
  unsigned long size;
  unsigned long used;
  unsigned long item_size;
  unsigned long alloc_size;
  long sign;
  long (*compare)(const void*, const void*);
};

extern struct heap heap(const unsigned long, const long, long (*)(const void*, const void*), const unsigned long);

extern int heap_resize(struct heap* const, const unsigned long);

extern int heap_insert(struct heap* const, const void* const);

extern void heap_pop(struct heap* const);

extern void heap_down(const struct heap* const, const unsigned long);

extern void heap_up(const struct heap* const, const unsigned long);

extern void heap_free(struct heap* const);

extern int heap_is_empty(const struct heap* const);

extern void* heap_peak(const struct heap* const, const unsigned long);

#endif // tN_a_OVDejTvT_qZjNU51_kyi3Ym__CB