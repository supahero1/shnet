#include "heap.h"

#include <stdlib.h>
#include <string.h>

struct heap heap(const unsigned long item_size, const long sign, long (*compare)(const void*, const void*)) {
  return (struct heap) {
    .size = item_size,
    .used = item_size,
    .item_size = item_size,
    .sign = sign,
    .compare = compare
  };
}

int heap_resize(struct heap* const heap, const unsigned long new_size) {
  void* const ptr = realloc(heap->array, new_size);
  if(ptr == NULL) {
    return heap_out_of_memory;
  }
  heap->array = ptr;
  heap->size = new_size;
  return heap_success;
}

int heap_add_item(struct heap* const heap, const void* const item) {
  if(heap->size - heap->used < heap->item_size) {
    if(heap_resize(heap, heap->used + heap->item_size) == heap_out_of_memory) {
      return heap_out_of_memory;
    }
  }
  if(heap->used == heap->item_size) {
    (void) memcpy(heap->array + heap->item_size, item, heap->item_size);
    goto out;
  }
  (void) memcpy(heap->array + heap->used, item, heap->item_size);
  heap_up(heap, heap->used);
  out:
  heap->used += heap->item_size;
  return heap_success;
}

void heap_pop(struct heap* const heap) {
  (void) memcpy(heap->array, heap->array + heap->item_size, heap->item_size);
  heap->used -= heap->item_size;
  if(heap->used != heap->item_size) {
    (void) memcpy(heap->array + heap->item_size, heap->array + heap->used, heap->item_size);
    (void) memcpy(heap->array + heap->used, heap->array, heap->item_size);
    heap_down(heap, heap->item_size);
    (void) memcpy(heap->array, heap->array + heap->used, heap->item_size);
  }
}

void heap_down(const struct heap* const heap, const unsigned long index) {
  unsigned long idx = index << 1;
  if(idx < heap->used) {
    (void) memcpy(heap->array, heap->array + index, heap->item_size);
    while(1) {
      const long left_diff = heap->compare(heap->array, heap->array + idx);
      if(idx + heap->item_size < heap->used) {
        if(heap->compare(heap->array      , heap->array + idx + heap->item_size) * heap->sign < 0 &&
           heap->compare(heap->array + idx, heap->array + idx + heap->item_size) * heap->sign < 0) {
          (void) memcpy(heap->array + (idx >> 1), heap->array + idx + heap->item_size, heap->item_size);
          idx += heap->item_size;
        } else if(left_diff * heap->sign < 0) {
          (void) memcpy(heap->array + (idx >> 1), heap->array + idx, heap->item_size);
        } else {
          idx >>= 1;
          break;
        }
      } else if(left_diff * heap->sign < 0) {
        (void) memcpy(heap->array + (idx >> 1), heap->array + idx, heap->item_size);
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
      (void) memcpy(heap->array + idx, heap->array, heap->item_size);
    }
  }
}

void heap_up(const struct heap* const heap, const unsigned long index) {
  unsigned long parent = ((index / heap->item_size) >> 1) * heap->item_size;
  if(heap->compare(heap->array + parent, heap->array + index) * heap->sign >= 0) {
    return;
  }
  unsigned long idx = index;
  (void) memcpy(heap->array, heap->array + index, heap->item_size);
  do {
    (void) memcpy(heap->array + idx, heap->array + parent, heap->item_size);
    if(parent == heap->item_size) {
      (void) memcpy(heap->array + heap->item_size, heap->array, heap->item_size);
      return;
    } else {
      idx = parent;
      parent = ((parent / heap->item_size) >> 1) * heap->item_size;
    }
  } while(heap->compare(heap->array + parent, heap->array) * heap->sign < 0);
  if(index != idx) {
    (void) memcpy(heap->array + idx, heap->array, heap->item_size);
  }
}