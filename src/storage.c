#include "error.h"
#include "storage.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>

void data_storage_free(struct data_storage* const storage) {
  if(storage->frames != NULL) {
    for(uint32_t i = 0; i < storage->len; ++i) {
      if(!storage->frames[i].dont_free) {
        free(storage->frames[i].data);
      }
    }
    free(storage->frames);
    storage->frames = NULL;
  }
  storage->len = 0;
}

int data_storage_add(struct data_storage* const storage, void* data, uint64_t size, const uint64_t offset, const enum data_storage_flags flags) {
  struct data_storage_frame* ptr;
  safe_execute(ptr = realloc(storage->frames, sizeof(struct data_storage_frame) * (storage->len + 1)), ptr == NULL, ENOMEM);
  if(ptr == NULL) {
    goto err;
  }
  storage->frames = ptr;
  
  if(!(flags & data_read_only)) {
    size -= offset;
    data = (char*) data + offset;
    void* data_ptr;
    safe_execute(data_ptr = malloc(size), data_ptr == NULL, ENOMEM);
    if(data_ptr == NULL) {
      goto err;
    }
    (void) memcpy(data_ptr, data, size);
    storage->frames[storage->len++] = (struct data_storage_frame) { data_ptr, 0, 0, size, 0 };
  } else {
    storage->frames[storage->len++] = (struct data_storage_frame) { data    , offset, 1, size, flags & data_dont_free };
  }
  return 0;
  
  err:
  if(!(flags & data_dont_free)) {
    free(data);
  }
  return -1;
}

int data_storage_drain(struct data_storage* const storage, const uint64_t amount) {
  storage->frames->offset += amount;
  if(storage->frames->offset == storage->frames->len) {
    if(!storage->frames->dont_free) {
      free(storage->frames->data);
    }
    if(--storage->len == 0) {
      return 1;
    }
    (void) memmove(storage->frames, storage->frames + 1, sizeof(struct data_storage_frame) * storage->len);
  }
  return 0;
}

void data_storage_finish(const struct data_storage* const storage) {
  if(!storage->frames->read_only && storage->frames->offset != 0) {
    (void) memmove(storage->frames->data, storage->frames->data + storage->frames->offset, storage->frames->len - storage->frames->offset);
    storage->frames->len -= storage->frames->offset;
    storage->frames->offset = 0;
    char* ptr;
    safe_execute(ptr = realloc(storage->frames->data, storage->frames->len), ptr == NULL, ENOMEM);
    if(ptr != NULL) {
      storage->frames->data = ptr;
    }
  }
}

int data_storage_is_empty(const struct data_storage* const storage) {
  return storage->len == 0;
}