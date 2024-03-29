#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>
#include <sys/mman.h>

#include <shnet/error.h>
#include <shnet/storage.h>

void data_storage_free_frame(const struct data_frame* const frame) {
  if(!frame->dont_free) {
    if(frame->mmaped) {
      (void) munmap(frame->data, frame->len);
    } else if(frame->file) {
      (void) close(frame->fd);
    } else {
      free(frame->data);
    }
  }
}

void data_storage_free_frame_err(const struct data_frame* const frame) {
  if(frame->free_onerr) {
    if(frame->mmaped) {
      (void) munmap(frame->data, frame->len);
    } else if(frame->file) {
      (void) close(frame->fd);
    } else {
      free(frame->data);
    }
  }
}

void data_storage_free(struct data_storage* const storage) {
  if(storage->frames != NULL) {
    for(uint32_t i = 0; i < storage->used; ++i) {
      data_storage_free_frame(&storage->frames[i]);
    }
    free(storage->frames);
    storage->frames = NULL;
  }
  storage->used = 0;
  storage->size = 0;
}

int data_storage_resize(struct data_storage* const storage, const uint32_t new_len) {
  if(new_len == storage->size) {
    return 0;
  }
  if(new_len == 0) {
    if(storage->frames != NULL) {
      free(storage->frames);
      storage->frames = NULL;
    }
    storage->used = 0;
    storage->size = 0;
    return 0;
  }
  void* const ptr = shnet_realloc(storage->frames, sizeof(*storage->frames) * new_len);
  if(ptr == NULL) {
    return -1;
  }
  storage->frames = ptr;
  storage->size = new_len;
  return 0;
}

int data_storage_add(struct data_storage* const storage, const struct data_frame* const frame) {
  if(storage->used >= storage->size && data_storage_resize(storage, storage->used + 1)) {
    return -1;
  }
  if(frame->offset == frame->len) {
    return 0;
  }
  if(!frame->read_only) {
    if(!frame->file) {
      const uint64_t len = frame->len - frame->offset;
      void* const data_ptr = shnet_malloc(len);
      if(data_ptr == NULL) {
        goto err;
      }
      (void) memcpy(data_ptr, frame->data + frame->offset, len);
      storage->frames[storage->used++] = (struct data_frame) {
        .data = data_ptr,
        .len = len
      };
    } else {
      void* data_ptr;
      safe_execute(data_ptr = mmap(NULL, frame->len, PROT_READ, MAP_PRIVATE, frame->fd, 0), data_ptr == MAP_FAILED, errno);
      if(data_ptr == MAP_FAILED) {
        goto err;
      }
      if(data_storage_add(storage, &((struct data_frame) {
        .data = data_ptr,
        .len = frame->len,
        .offset = frame->offset,
        .free_onerr = 1,
        .mmaped = 1
      }))) {
        goto err;
      }
    }
    data_storage_free_frame(frame);
  } else {
    storage->frames[storage->used++] = *frame;
  }
  return 0;
  
  err:
  data_storage_free_frame_err(frame);
  return -1;
}

#define frame storage->frames

void data_storage_drain(struct data_storage* const storage, const uint64_t amount) {
  if(storage->used == 0) {
    assert(amount == 0);
    return;
  }
  frame->offset += amount;
  if(frame->offset == frame->len) {
    data_storage_free_frame(frame);
    --storage->used;
    (void) memmove(frame, frame + 1, sizeof(*frame) * storage->used);
  }
}

void data_storage_finish(const struct data_storage* const storage) {
  if(storage->used != 0 && !frame->read_only && frame->offset != 0) {
    frame->len -= frame->offset;
    (void) memmove(frame->data, frame->data + frame->offset, frame->len);
    frame->offset = 0;
    char* const ptr = shnet_realloc(frame->data, frame->len);
    if(ptr != NULL) {
      frame->data = ptr;
    }
  }
}

#undef frame

int data_storage_is_empty(const struct data_storage* const storage) {
  return storage->used == 0;
}

uint64_t data_storage_size(const struct data_storage* const storage) {
  uint64_t sum = 0;
  for(uint32_t i = 0; i < storage->used; ++i) {
    sum += storage->frames[i].len - storage->frames[i].offset;
  }
  return sum;
}
