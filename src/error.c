#include <errno.h>
#include <stdlib.h>

#include <shnet/error.h>

void* shnet_malloc(const size_t size) {
  void* ptr;
  safe_execute(ptr = malloc(size), ptr == NULL, ENOMEM);
  return ptr;
}

void* shnet_calloc(const size_t num, const size_t size) {
  void* ptr;
  safe_execute(ptr = calloc(num, size), ptr == NULL, ENOMEM);
  return ptr;
}

void* shnet_realloc(void* const in, const size_t size) {
  void* ptr;
  safe_execute(ptr = realloc(in, size), ptr == NULL, ENOMEM);
  return ptr;
}
