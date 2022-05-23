#define TEST_NO_ERR_HANDLER
#include <shnet/test.h>

#include <stdlib.h>

int error_handler(int code, int count) {
  if(count == 7) {
    return 1;
  }
  return !code;
}

int main() {
  test_begin("error");
  int err = 5;
  safe_execute((void) err, 1, 0);
  assert(err == 5);
  safe_execute(--err, 1, err);
  assert(err == 0);
  test_end();
  
  test_begin("error count");
  err = 10;
  safe_execute(--err, 1, err);
  assert(err == 2);
  test_end();
  
  test_begin("error coverage");
  void* ptr = shnet_malloc(256);
  assert(ptr);
  free(ptr);
  ptr = shnet_calloc(1, 256);
  assert(ptr);
  ptr = shnet_realloc(ptr, 128);
  assert(ptr);
  free(ptr);
  test_end();
  
  return 0;
}
