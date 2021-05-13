#include "misc.h"

#include <stdlib.h>
#include <string.h>
#include <stdint.h>

int contmem(struct contmem* const contmem, const unsigned long max_items, const unsigned long item_size, const unsigned long tolerance) {
  struct contmem_block* const ptr = malloc(sizeof(struct contmem_block) + max_items * item_size);
  if(ptr == NULL) {
    return contmem_out_of_memory;
  }
  ptr->next = NULL;
  ptr->prev = NULL;
  ptr->used = 0;
  *contmem = (struct contmem) {
    .tail = ptr,
    .block_size = max_items * item_size,
    .item_size = item_size,
    .tolerance = tolerance
  };
  return contmem_success;
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
  int output = 0;
  if(contmem_last(contmem) != mem) {
    (void) memcpy(mem, contmem_last(contmem), contmem->item_size);
    output = 1;
  }
  contmem->tail->used -= contmem->item_size;
  if(contmem->tail->used == 0 && contmem->tail->prev != NULL) {
    contmem->tail = contmem->tail->prev;
  }
  if(contmem->tail->next != NULL && contmem->block_size - contmem->tail->used <= contmem->tolerance) {
    free(contmem->tail->next);
    contmem->tail->next = NULL;
  }
  return output;
}

void* contmem_last(struct contmem* const contmem) {
  return (char*)(contmem->tail + 1) + (contmem->tail->used - contmem->item_size);
}

void contmem_free(struct contmem* const contmem) {
  struct contmem_block* block = contmem->tail;
  struct contmem_block* prev;
  do {
    prev = block->prev;
    free(block);
    block = prev;
  } while(block != NULL);
}