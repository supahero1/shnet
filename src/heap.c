#include "heap.h"
#include "error.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>

int heap_resize(struct heap* const heap, const uint64_t new_size) {
  void* ptr;
  safe_execute(ptr = realloc(heap->array, new_size), ptr == NULL, ENOMEM);
  if(ptr == NULL) {
    return -1;
  }
  heap->array = ptr;
  heap->size = new_size;
  return 0;
}

int heap_insert(struct heap* const heap, const void* const item) {
  if(heap->size - heap->used < heap->item_size) {
    if(heap_resize(heap, heap->used + heap->item_size) == -1) {
      return -1;
    }
  }
  if(heap_is_empty(heap)) {
    (void) memcpy(heap->array + heap->item_size, item, heap->item_size);
  } else {
    (void) memcpy(heap->array + heap->used, item, heap->item_size);
    heap_up(heap, heap->used);
  }
  heap->used += heap->item_size;
  return 0;
}

void heap_pop(struct heap* const heap) {
  (void) memcpy(heap->array, heap->array + heap->item_size, heap->item_size);
  heap->used -= heap->item_size;
  if(!heap_is_empty(heap)) {
    (void) memcpy(heap->array + heap->item_size, heap->array + heap->used, heap->item_size);
    (void) memcpy(heap->array + heap->used, heap->array, heap->item_size);
    heap_down(heap, heap->item_size);
    (void) memcpy(heap->array, heap->array + heap->used, heap->item_size);
  }
}

void heap_down(const struct heap* const heap, const uint64_t index) {
  uint64_t idx = index << 1;
  if(idx >= heap->used) {
    return;
  }
  (void) memcpy(heap->array, heap->array + index, heap->item_size);
  while(1) {
    const int left_diff = heap->compare(heap->array, heap->array + idx);
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

void heap_up(const struct heap* const heap, const uint64_t index) {
  uint64_t parent = ((index / heap->item_size) >> 1) * heap->item_size;
  if(parent < heap->item_size || heap->compare(heap->array + parent, heap->array + index) * heap->sign >= 0) {
    return;
  }
  uint64_t idx = index;
  (void) memcpy(heap->array, heap->array + index, heap->item_size);
  do {
    (void) memcpy(heap->array + idx, heap->array + parent, heap->item_size);
    if(parent == heap->item_size) {
      (void) memcpy(heap->array + heap->item_size, heap->array, heap->item_size);
      return;
    }
    idx = parent;
    parent = ((parent / heap->item_size) >> 1) * heap->item_size;
  } while(heap->compare(heap->array + parent, heap->array) * heap->sign < 0);
  if(index != idx) {
    (void) memcpy(heap->array + idx, heap->array, heap->item_size);
  }
}

void heap_free(struct heap* const heap) {
  if(heap->array != NULL) {
    free(heap->array);
    heap->array = NULL;
  }
  heap->size = heap->item_size;
  heap->used = heap->item_size;
}

int heap_is_empty(const struct heap* const heap) {
  return heap->used == heap->item_size;
}

void* heap_peak(const struct heap* const heap, const uint64_t idx) {
  return heap->array + heap->item_size * (idx + 1);
}