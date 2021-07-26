#include "hash_table.h"

#include <string.h>
#include <stdlib.h>

void http_hash_table_insert(struct http_hash_table* const table, char* const key, void* const data, http_hash_table_func_t func) {
  uint64_t index = FNV_1a(key, 0) % table->size;
  while(table->table[index].key != NULL) {
    index = (index + 1) % table->size;
  }
  table->table[index].key = key;
  table->table[index].data = data;
  table->table[index].func = func;
  ++table->used;
}

struct http_hash_table_entry* http_hash_table_find(const struct http_hash_table* const table, const char* const key) {
  const size_t len = strlen(key);
  uint64_t index = FNV_1a(key, len) % table->size;
  if(table->table[index].key == NULL) {
    return NULL;
  }
  while(1) {
    if(table->table[index].key == NULL) {
      return NULL;
    }
    if(len == strlen(table->table[index].key) && strncmp(table->table[index].key, key, len) == 0) {
      return table->table + index;
    }
    index = (index + 1) % table->size;
  }
}

int http_hash_table_init(struct http_hash_table* const table, const uint64_t size) {
  table->table = calloc(size, sizeof(struct http_hash_table_entry));
  if(table->table == NULL) {
    return -1;
  }
  table->size = size;
  return 0;
}

void http_hash_table_free(struct http_hash_table* const table) {
  free(table->table);
}



uint64_t FNV_1a(const char* const str, size_t len) {
  if(len == 0) {
    len = strlen(str);
  }
  uint64_t hash = 0xCBF29CE484222325;
  for(size_t i = 0; i < len; ++i) {
    hash ^= str[i];
    hash *= 0x00000100000001B3;
  }
  return hash;
}