#include "refheap.h"

#include <stdlib.h>
#include <string.h>
#include <stdint.h>

struct heap refheap(const unsigned long item_size, const long sign, long (*compare)(const void*, const void*), const unsigned long alloc_size) {
  return (struct heap) {
    .size = item_size + sizeof(long),
    .used = item_size + sizeof(long),
    .item_size = item_size + sizeof(long),
    .alloc_size = alloc_size,
    .sign = sign,
    .compare = compare
  };
}

unsigned long refheap_ref_to_idx(const struct heap* const heap, const void* const ref) {
  return (uintptr_t) ref - sizeof(long) - (uintptr_t) heap->array;
}

int refheap_resize(struct heap* const heap, unsigned long new_size) {
  new_size = (new_size / (heap->item_size - sizeof(long))) * heap->item_size;
  void* const ptr = realloc(heap->array, new_size);
  if(ptr == NULL) {
    return heap_out_of_memory;
  }
  heap->array = ptr;
  heap->size = new_size;
  return heap_success;
}

int refheap_insert(struct heap* const heap, const void* const item, void** const ref) {
  if(heap->size - heap->used < heap->item_size) {
    if(refheap_resize(heap, heap->used + heap->item_size * heap->alloc_size) == heap_out_of_memory) {
      return heap_out_of_memory;
    }
  }
  if(refheap_is_empty(heap)) {
    (void) memcpy(heap->array + heap->item_size + sizeof(long), item, heap->item_size - sizeof(long));
    ((struct refheap_ref*)(heap->array + heap->item_size))->ref = ref;
    if(ref != NULL) {
      *ref = heap->array + heap->item_size + sizeof(long);
    }
  } else {
    (void) memcpy(heap->array + heap->used + sizeof(long), item, heap->item_size - sizeof(long));
    ((struct refheap_ref*)(heap->array + heap->used))->ref = ref;
    if(ref != NULL) {
      *ref = heap->array + heap->used + sizeof(long);
    }
    refheap_up(heap, heap->used);
  }
  heap->used += heap->item_size;
  return heap_success;
}

static void refheap_swap(const struct heap* const heap, void* const a1, void* const a2) {
  (void) memcpy(a1, a2, heap->item_size);
  void** const ref = ((struct refheap_ref*) a1)->ref;
  if(ref != NULL) {
    *ref = (char*) a1 + sizeof(long);
  }
}

void refheap_pop(struct heap* const heap) {
  (void) memcpy(heap->array, heap->array + heap->item_size, heap->item_size);
  heap->used -= heap->item_size;
  if(!refheap_is_empty(heap)) {
    refheap_swap(heap, heap->array + heap->item_size, heap->array + heap->used);
    (void) memcpy(heap->array + heap->used, heap->array, heap->item_size);
    (void) refheap_down(heap, heap->item_size);
    (void) memcpy(heap->array, heap->array + heap->used, heap->item_size);
  }
}

void refheap_down(const struct heap* const heap, const unsigned long index) {
  unsigned long idx = index << 1;
  if(idx >= heap->used) {
    return;
  }
  (void) memcpy(heap->array, heap->array + index, heap->item_size);
  while(1) {
    const long left_diff = heap->compare(heap->array + sizeof(long), heap->array + idx + sizeof(long));
    if(idx + heap->item_size < heap->used) {
      if(heap->compare(heap->array       + sizeof(long), heap->array + idx + heap->item_size + sizeof(long)) * heap->sign < 0 &&
         heap->compare(heap->array + idx + sizeof(long), heap->array + idx + heap->item_size + sizeof(long)) * heap->sign < 0) {
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

void refheap_up(const struct heap* const heap, const unsigned long index) {
  unsigned long parent = ((index / heap->item_size) >> 1) * heap->item_size;
  if(parent < heap->item_size || heap->compare(heap->array + parent + sizeof(long), heap->array + index + sizeof(long)) * heap->sign >= 0) {
    return;
  }
  unsigned long idx = index;
  (void) memcpy(heap->array, heap->array + index, heap->item_size);
  do {
    refheap_swap(heap, heap->array + idx, heap->array + parent);
    if(parent == heap->item_size) {
      refheap_swap(heap, heap->array + heap->item_size, heap->array);
      return;
    }
    idx = parent;
    parent = ((parent / heap->item_size) >> 1) * heap->item_size;
  } while(heap->compare(heap->array + parent + sizeof(long), heap->array + sizeof(long)) * heap->sign < 0);
  if(index != idx) {
    refheap_swap(heap, heap->array + idx, heap->array);
  }
}

void refheap_delete(struct heap* const heap, void* const ref) {
  heap->used -= heap->item_size;
  if(!refheap_is_empty(heap)) {
    refheap_swap(heap, (char*) ref - sizeof(long), heap->array + heap->used);
    refheap_down(heap, refheap_ref_to_idx(heap, ref));
  }
}

void refheap_free(struct heap* const heap) {
  heap_free(heap);
}

int refheap_is_empty(const struct heap* const heap) {
  return heap_is_empty(heap);
}

void* refheap_peak(const struct heap* const heap, const unsigned long idx) {
  return (char*) heap_peak(heap, idx) + sizeof(long);
}