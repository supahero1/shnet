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
  void* ptr;
  safe_execute(ptr = realloc(storage->frames, sizeof(*storage->frames) * new_len), ptr == NULL, ENOMEM);
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
  if(!frame->read_only) {
    if(!frame->file) {
      const uint64_t len = frame->len - frame->offset;
      void* data_ptr;
      safe_execute(data_ptr = malloc(len), data_ptr == NULL, ENOMEM);
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

int data_storage_drain(struct data_storage* const storage, const uint64_t amount) {
  if(storage->used == 0) {
    /* Do not defy the rules. */
    assert(amount == 0);
    return 1;
  }
  storage->frames->offset += amount;
  if(storage->frames->offset == storage->frames->len) {
    data_storage_free_frame(storage->frames);
    if(--storage->used == 0) {
      return 1;
    }
    (void) memmove(storage->frames, storage->frames + 1, sizeof(*storage->frames) * storage->used);
  }
  return 0;
}

void data_storage_finish(const struct data_storage* const storage) {
  if(storage->used != 0 && !storage->frames->read_only && storage->frames->offset != 0) {
    storage->frames->len -= storage->frames->offset;
    (void) memmove(storage->frames->data, storage->frames->data + storage->frames->offset, storage->frames->len);
    storage->frames->offset = 0;
    char* ptr;
    safe_execute(ptr = realloc(storage->frames->data, storage->frames->len), ptr == NULL, ENOMEM);
    if(ptr != NULL) {
      storage->frames->data = ptr;
    }
  }
}

int data_storage_is_empty(const struct data_storage* const storage) {
  return storage->used == 0;
}