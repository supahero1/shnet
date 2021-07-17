#ifndef __P_JY2t7Kexb_j7C__C__oEtzLoCNh9
#define __P_JY2t7Kexb_j7C__C__oEtzLoCNh9 1

#include <stdint.h>
#include <stddef.h>

/* A specialised open-addressed hash table for HTTP servers. Deletion is impossible. */

typedef void (*http_hash_table_value_t)(void*, void*, void*, void*);

struct http_hash_table_entry {
  char* key;
  void (*value)(void*, void*, void*, void*);
};

struct http_hash_table {
  struct http_hash_table_entry* table;
  uint64_t used;
  uint64_t size;
};

extern void http_hash_table_insert(struct http_hash_table* const, char* const, http_hash_table_value_t);

extern http_hash_table_value_t http_hash_table_find(const struct http_hash_table* const, const char* const);

extern int http_hash_table_init(struct http_hash_table* const, const uint64_t);

extern void http_hash_table_free(struct http_hash_table* const);


extern uint64_t FNV_1a(const char* const, size_t);

#endif // __P_JY2t7Kexb_j7C__C__oEtzLoCNh9