#ifndef X3TIrqm_E__OmXwd_gsJr74__ASz3mxQ
#define X3TIrqm_E__OmXwd_gsJr74__ASz3mxQ 1

#define _GNU_SOURCE
#include <dlfcn.h>
#undef _GNU_SOURCE

#include <time.h>
#include <dlfcn.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <setjmp.h>
#include <stdlib.h>
#include <stdint.h>
#include <assert.h>
#include <signal.h>
#include <pthread.h>
#include <semaphore.h>

#ifndef TEST_NO_ERR_HANDLER
#include <shnet/error.h>

int error_handler(int c) {
  if(c == EINTR) return 0;
  return -1;
}
#endif // TEST_NO_ERR_HANDLER

void test_seed_random(void) {
  struct timespec tp;
  (void) clock_gettime(CLOCK_REALTIME, &tp);
  srand(tp.tv_nsec + tp.tv_sec * 1000000000);
}

#define begin_test(a) assert(printf("Testing " a "... ")>=0);assert(fflush(stdout)==0)
#define end_test() assert(printf("done\n")>=0)

static pthread_once_t _test_waiting_once = PTHREAD_ONCE_INIT;
static sem_t _test_sem;

static void _test_waiting_destroy(void) {
  assert(!sem_destroy(&_test_sem));
}

static void _test_waiting_init(void) {
  assert(!sem_init(&_test_sem, 0, 0));
  assert(!atexit(_test_waiting_destroy));
}

void test_wait(void) {
  (void) pthread_once(&_test_waiting_once, _test_waiting_init);
  assert(!sem_wait(&_test_sem));
}

void test_wake(void) {
  (void) pthread_once(&_test_waiting_once, _test_waiting_init);
  assert(!sem_post(&_test_sem));
}

void test_sleep(const uint64_t ms) {
  (void) nanosleep(&((struct timespec) {
    .tv_sec = ms / 1000,
    .tv_nsec = (ms % 1000) * 1000000
  }), NULL);
}

static volatile char _test_var;
static jmp_buf _test_jump;

static void _test_sigsegv(int code) {
  siglongjmp(_test_jump, 1);
}

static pthread_once_t _test_segv_once = PTHREAD_ONCE_INIT;

static void _test_segv_init(void) {
  assert(!sigaction(SIGSEGV, &((struct sigaction) {
    .sa_handler = _test_sigsegv
  }), NULL));
}

void test_expect_segfault(const void* const ptr) {
  /*
   * Valgrind (and most other tools) do not detect memory leaks
   * created by mmap(). Fortunately though, man pages mention
   * that further access to a munmap()'ed memory region will
   * result in a SIGSEGV. As a result, this function triggers
   * the signal, and if it succeeds, the program flow continues.
   * If it fails for some reason (munmap() wasn't called), the
   * program will be put in an infinite loop.
   */
  (void) pthread_once(&_test_segv_once, _test_segv_init);
  if(!sigsetjmp(_test_jump, 1)) {
    _test_var = *(char*)ptr;
    while(1) {
      __asm__ volatile("rep; nop");
    }
  }
}

void test_expect_no_segfault(const void* const ptr) {
  _test_var = *(char*)ptr;
}

/*
 * Little something to cause errors on purpose.
 */

#define test_register(ret, name, types, args) \
int fail_##name = -1; \
ret name types { \
  static ret (*_real) types = NULL; \
  if(_real == NULL) { \
    _real = ( ret (*) types ) dlsym(RTLD_NEXT, #name); \
    if(_real == NULL) { \
      puts(dlerror()); \
      assert(0); \
    } \
  } \
  if(fail_##name != -1) { \
    if(!--fail_##name) { \
      fail_##name = -1; \
      errno = EAGAIN; \
      return -1; \
    } \
  } \
  return _real args ; \
}

#define test_error_at(name, num) fail_##name = num
#define test_error(name) test_error_at(name, 1)

#endif // X3TIrqm_E__OmXwd_gsJr74__ASz3mxQ