#include "refheap.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

int refheap_resize(struct heap* const heap, const uint64_t new_size) {
  return heap_resize(heap, new_size);
}

int refheap_insert(struct heap* const heap, const void* const item, uint64_t* const ref) {
  if(heap->size - heap->used < heap->item_size) {
    if(refheap_resize(heap, heap->used + heap->item_size) == -1) {
      return -1;
    }
  }
  if(refheap_is_empty(heap)) {
    (void) memcpy(heap->array + heap->item_size + sizeof(uint64_t**), item, heap->item_size - sizeof(uint64_t**));
    *(uint64_t**)(heap->array + heap->item_size) = ref;
    if(ref != NULL) {
      *ref = heap->item_size;
    }
  } else {
    (void) memcpy(heap->array + heap->used + sizeof(uint64_t**), item, heap->item_size - sizeof(uint64_t**));
    *(uint64_t**)(heap->array + heap->used) = ref;
    if(ref != NULL) {
      *ref = heap->used;
    }
    refheap_up(heap, heap->used);
  }
  heap->used += heap->item_size;
  return 0;
}

static void refheap_swap(const struct heap* const heap, void* const a1, void* const a2) {
  (void) memcpy(a1, a2, heap->item_size);
  uint64_t* const ref = *(uint64_t**)a1;
  if(ref != NULL) {
    *ref = heap_abs_idx(heap, a1);
  }
}

void* refheap_pop(struct heap* const heap) {
  (void) memcpy(heap->array, heap->array + heap->item_size, heap->item_size);
  heap->used -= heap->item_size;
  if(!refheap_is_empty(heap)) {
    refheap_swap(heap, heap->array + heap->item_size, heap->array + heap->used);
    (void) memcpy(heap->array + heap->used, heap->array, heap->item_size);
    refheap_down(heap, heap->item_size);
    (void) memcpy(heap->array, heap->array + heap->used, heap->item_size);
  }
  return heap->array + sizeof(uint64_t**);
}

void refheap_pop_(struct heap* const heap) {
  heap->used -= heap->item_size;
  if(!refheap_is_empty(heap)) {
    refheap_swap(heap, heap->array + heap->item_size, heap->array + heap->used);
    refheap_down(heap, heap->item_size);
  }
}

void refheap_down(const struct heap* const heap, const uint64_t index) {
  uint64_t idx = index << 1;
  if(idx >= heap->used) {
    return;
  }
  (void) memcpy(heap->array, heap->array + index, heap->item_size);
  while(1) {
    const int left_diff = heap->compare(heap->array + sizeof(uint64_t**), heap->array + idx + sizeof(uint64_t**));
    if(idx + heap->item_size < heap->used) {
      if(heap->compare(heap->array       + sizeof(uint64_t**), heap->array + idx + heap->item_size + sizeof(uint64_t**)) * heap->sign < 0 &&
         heap->compare(heap->array + idx + sizeof(uint64_t**), heap->array + idx + heap->item_size + sizeof(uint64_t**)) * heap->sign < 0) {
        refheap_swap(heap, heap->array + (idx >> 1), heap->array + idx + heap->item_size);
        idx += heap->item_size;
      } else if(left_diff * heap->sign < 0) {
        refheap_swap(heap, heap->array + (idx >> 1), heap->array + idx);
      } else {
        idx >>= 1;
        break;
      }
    } else if(left_diff * heap->sign < 0) {
      refheap_swap(heap, heap->array + (idx >> 1), heap->array + idx);
    } else {
      idx >>= 1;
      break;
    }
    idx <<= 1;
    if(idx >= heap->used) {
      idx >>= 1;
      break;
    }
  }
  if(idx != index) {
    refheap_swap(heap, heap->array + idx, heap->array);
  }
}

void refheap_up(const struct heap* const heap, const uint64_t index) {
  uint64_t parent = ((index / heap->item_size) >> 1) * heap->item_size;
  if(parent < heap->item_size || heap->compare(heap->array + parent + sizeof(uint64_t**), heap->array + index + sizeof(uint64_t**)) * heap->sign >= 0) {
    return;
  }
  uint64_t idx = index;
  (void) memcpy(heap->array, heap->array + index, heap->item_size);
  do {
    refheap_swap(heap, heap->array + idx, heap->array + parent);
    if(parent == heap->item_size) {
      refheap_swap(heap, heap->array + heap->item_size, heap->array);
      return;
    }
    idx = parent;
    parent = ((parent / heap->item_size) >> 1) * heap->item_size;
  } while(heap->compare(heap->array + parent + sizeof(uint64_t**), heap->array + sizeof(uint64_t**)) * heap->sign < 0);
  if(index != idx) {
    refheap_swap(heap, heap->array + idx, heap->array);
  }
}

void refheap_delete(struct heap* const heap, const uint64_t ref) {
  if(ref == heap->item_size) {
    refheap_pop_(heap);
  } else {
    heap->used -= heap->item_size;
    if(ref == heap->used) {
      return;
    }
    refheap_swap(heap, heap->array + ref, heap->array + heap->used);
    refheap_down(heap, ref);
    refheap_up(heap, ref);
  }
}

void refheap_free(struct heap* const heap) {
  heap_free(heap);
}

int refheap_is_empty(const struct heap* const heap) {
  return heap_is_empty(heap);
}

void* refheap_peak(const struct heap* const heap, const uint64_t idx) {
  return (char*) heap_peak(heap, idx) + sizeof(uint64_t**);
}

void* refheap_peak_rel(const struct heap* const heap, const uint64_t idx) {
  return (char*) heap_peak_rel(heap, idx) + sizeof(uint64_t**);
}

uint64_t refheap_abs_idx(const struct heap* const heap, const void* const item) {
  return heap_abs_idx(heap, (char*) item - sizeof(uint64_t**));
}

void refheap_inject(const struct heap* const heap, const uint64_t idx, uint64_t* const ref) {
  *(uint64_t**) heap_peak(heap, idx) = ref;
}

void refheap_inject_rel(const struct heap* const heap, const uint64_t idx, uint64_t* const ref) {
  *(uint64_t**) heap_peak_rel(heap, idx) = ref;
}
