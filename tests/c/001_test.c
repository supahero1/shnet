#include <shnet/test.h>

#include <errno.h>
#include <unistd.h>

test_register(void*, shnet_malloc, (const size_t a), (a))
test_register(void*, shnet_calloc, (const size_t a, const size_t b), (a, b))
test_register(void*, shnet_realloc, (void* const a, const size_t b), (a, b))
test_register(int, pipe, (int a[2]), (a))

int main() {
  int fds[2];
  
  test_begin("test check");
  test_error_check(void*, shnet_malloc, (0xbad));
  test_error_check(void*, shnet_calloc, (0xbad, 0xbad));
  test_error_check(void*, shnet_realloc, ((void*) 0xbad, 0xbad));
  test_error_check(int, pipe, (fds));
  test_end();
  
  test_begin("test register");
  assert(pipe(fds) != -1);
  close(fds[0]);
  close(fds[1]);
  test_error_set(pipe, 2);
  test_error_set_retval(pipe, -1);
  assert(test_error_get(pipe) == 2);
  assert(pipe(fds) != -1);
  close(fds[0]);
  close(fds[1]);
  assert(test_error_get(pipe) == 1);
  assert(pipe(fds) == -1);
  assert(errno == ECANCELED);
  errno = 0;
  assert(pipe(fds) != -1);
  assert(errno == 0);
  close(fds[0]);
  close(fds[1]);
  test_end();
  
  return 0;
}
