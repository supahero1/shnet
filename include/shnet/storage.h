#ifndef _shnet_storage_h_
#define _shnet_storage_h_ 1

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

struct data_frame {
  union {
    char* data;
    int fd;
  };
  uint64_t len:61;
  uint64_t read_only:1;
  uint64_t dont_free:1;
  uint64_t free_onerr:1;
  uint64_t offset:61;
  uint64_t mmaped:1;
  uint64_t file:1;
  uint64_t _unused:1;
};

struct data_storage {
  struct data_frame* frames;
  uint32_t used;
  uint32_t size;
};

extern void data_storage_free_frame(const struct data_frame* const);

extern void data_storage_free_frame_err(const struct data_frame* const);

extern void data_storage_free(struct data_storage* const);

extern int  data_storage_resize(struct data_storage* const, const uint32_t);

extern int  data_storage_add(struct data_storage* const, const struct data_frame* const);

extern void data_storage_drain(struct data_storage* const, const uint64_t);

extern void data_storage_finish(const struct data_storage* const);

extern int  data_storage_is_empty(const struct data_storage* const);

extern uint64_t data_storage_size(const struct data_storage* const);

#ifdef __cplusplus
}
#endif

#endif /* _shnet_storage_h_ */
