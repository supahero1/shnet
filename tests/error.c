#define TEST_NO_ERR_HANDLER
#include <shnet/tests.h>
#include <shnet/error.h>

int error_handler(int code, int count) {
  return !code;
}

int main() {
  begin_test("error");
  int err = 5;
  safe_execute((void) err, 1, 0);
  assert(err == 5);
  safe_execute(--err, 1, err);
  assert(err == 0);
  end_test();
  return 0;
}