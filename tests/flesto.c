#include "tests.h"

#include <shnet/flesto.h>

int main() {
  TEST_BEGIN;
  struct flesto f;
  (void) f;
  printf_debug("this is a test and a number %d", 347);
  printf_debug("plain text");
  TEST_PASS;
  return 0;
}