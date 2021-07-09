#include "contmem.h"

#include <stdlib.h>
#include <string.h>

int contmem(struct contmem* const contmem, const uint64_t max_items, const uint64_t item_size, const uint64_t tolerance) {
  struct contmem_block* const ptr = malloc(sizeof(struct contmem_block) + max_items * item_size);
  if(ptr == NULL) {
    return -1;
  }
  contmem->tail = ptr;
  contmem->block_size = max_items * item_size;
  contmem->item_size = item_size;
  contmem->tolerance = tolerance;
  return 0;
}

void* contmem_get(struct contmem* const contmem) {
  if(contmem->tail->used == contmem->block_size) {
    if(contmem->tail->next != NULL) {
      contmem->tail = contmem->tail->next;
    } else {
      struct contmem_block* const ptr = malloc(sizeof(struct contmem_block) + contmem->block_size);
      if(ptr == NULL) {
        return NULL; 
      }
      ptr->next = NULL;
      ptr->prev = contmem->tail;
      ptr->used = 0;
      contmem->tail->next = ptr;
      contmem->tail = ptr;
    }
  }
  contmem->tail->used += contmem->item_size;
  return contmem_last(contmem);
}

int contmem_pop(struct contmem* const contmem, void* const mem) {
  const int output = contmem_last(contmem) != mem;
  if(output) {
    (void) memcpy(mem, contmem_last(contmem), contmem->item_size);
  }
  contmem_pop_cleanup(contmem);
  return output;
}

void contmem_pop_cleanup(struct contmem* const contmem) {
  contmem->tail->used -= contmem->item_size;
  if(contmem->tail->used == 0 && contmem->tail->prev != NULL) {
    contmem->tail = contmem->tail->prev;
  }
  if(contmem->tail->next != NULL && contmem->block_size - contmem->tail->used <= contmem->tolerance) {
    free(contmem->tail->next);
    contmem->tail->next = NULL;
  }
}

void* contmem_last(struct contmem* const contmem) {
  return (char*)(contmem->tail + 1) + (contmem->tail->used - contmem->item_size);
}

void contmem_free(struct contmem* const contmem) {
  if(contmem->tail == NULL) {
    return;
  }
  struct contmem_block* block = contmem->tail;
  struct contmem_block* prev;
  do {
    prev = block->prev;
    free(block);
    block = prev;
  } while(block != NULL);
  contmem->tail = NULL;
}