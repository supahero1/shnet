#ifndef _shnet_test_h_
#define _shnet_test_h_ 1

#ifdef __cplusplus
#error This header file is not compatible with C++
#endif

#define _GNU_SOURCE
#include <dlfcn.h>
#include <stdio.h>
#include <assert.h>
#include <stdint.h>

#include <shnet/error.h>

#ifndef TEST_NO_ERR_HANDLER

#include <errno.h>

int error_handler(int e, int c) {
  (void) c;
  if(e == EINTR || e == 0) return 0;
  return -1;
}

#endif


extern void test_seed_random(void);


extern void test_begin(const char* const);

extern void test_end(void);


extern void test_wait(void);

extern void test_mutex_wait(void);

extern void test_wake(void);

extern void test_mutex_wake(void);

extern void test_sleep(const uint64_t);


extern void test_expect_segfault(const void* const);

extern void test_expect_no_segfault(const void* const);


#define test_error_get_errno(name) _fail_##name##_errno
#define test_error_set_errno(name, code) test_error_get_errno(name) = code

#define test_error_get_retval(name) _fail_##name##_retval
#define test_error_set_retval(name, code) test_error_get_retval(name) = code

#define test_error_get(name) _fail_##name
#define test_error_set(name, num) test_error_get(name) = num
#define test_error(name) test_error_set(name, 1)

#define test_register(ret, name, types, args) \
uint32_t test_error_set(name, 0); \
int test_error_set_errno(name, ECANCELED); \
ret test_error_set_retval(name, (ret) -1); \
ret (*_real_##name) types = NULL; \
ret name types { \
  if(_real_##name == NULL) { \
    _real_##name = ( ret (*) types ) dlsym(RTLD_NEXT, #name); \
    if(_real_##name == NULL) { \
      _real_##name = ( ret (*) types ) dlsym(RTLD_DEFAULT, #name); \
      if(_real_##name == NULL) { \
        puts(dlerror()); \
        assert(0 && "shnet dlsym error for function " #name); \
      } \
    } \
  } \
  if(test_error_get(name) != 0 && !--test_error_get(name)) { \
    errno = test_error_get_errno(name); \
    return test_error_get_retval(name); \
  } \
  return _real_##name args ; \
}

#define test_error_check(ret, name, args) \
do { \
  uint32_t err_save = test_error_get(name); \
  test_error(name); \
  ret retval_save = test_error_get_retval(name); \
  test_error_set_retval(name, (ret) -0xdababe); \
  int errno_save = errno; \
  int err_errno_save = test_error_get_errno(name); \
  test_error_set_errno(name, -0xdababe); \
  assert(name args == (ret) -0xdababe); \
  assert(errno == -0xdababe); \
  test_error_set_retval(name, retval_save); \
  errno = errno_save; \
  test_error_set_errno(name, err_errno_save); \
  test_error_set(name, err_save); \
} while(0)

#endif /* _shnet_test_h_ */
