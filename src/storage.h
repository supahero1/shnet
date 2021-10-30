#ifndef PtC_TO_Ikc_NvoKymeL0_eZfvItFDVXF
#define PtC_TO_Ikc_NvoKymeL0_eZfvItFDVXF 1

#include <stdint.h>

enum data_storage_flags {
  data_dont_free = 1,
  data_read_only
};

struct data_storage_frame {
  char* data;
  uint64_t offset:63;
  uint64_t read_only:1;
  uint64_t len:63;
  uint64_t dont_free:1;
};

struct __attribute__((__packed__)) data_storage;

struct data_storage {
  struct data_storage_frame* frames;
  uint32_t len;
};

extern void data_storage_free(struct data_storage* const);

extern int  data_storage_add(struct data_storage* const, void*, uint64_t, const uint64_t, const enum data_storage_flags);

extern int  data_storage_drain(struct data_storage* const, const uint64_t);

extern void data_storage_finish(const struct data_storage* const);

extern int  data_storage_is_empty(const struct data_storage* const);

#endif // PtC_TO_Ikc_NvoKymeL0_eZfvItFDVXF