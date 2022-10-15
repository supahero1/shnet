#include <shnet/test.h>
#include <shnet/threads.h>

#ifndef SHNET_TEST_VALGRIND

#define safety_timeout ((uint32_t)300)

#else

#define safety_timeout ((uint32_t)3000)

#endif

void* assert_0(void* data) {
  assert(0);
}

void* cb(void* data) {
  test_sleep(safety_timeout);
  assert(0);
}

void* cb_wake(void* data) {
  test_wake();
  test_sleep(safety_timeout);
  assert(0);
}

void* cb_retval(void* data) {
  return data;
}

void* cb_stop_self(void* data) {
  pthread_detach(pthread_self());
  pthread_cancel(pthread_self());
  test_sleep(safety_timeout);
  assert(0);
}

void* cb_stop_sync_self(void* data) {
  pthread_cancel_sync(pthread_self());
  return NULL;
}

void* cb_stop_async_self(void* data) {
  pthread_cancel_async(pthread_self());
  return NULL;
}

void* cb_cancel_on(void* data) {
  pthread_cancel_off();
  pthread_cancel_on();
  test_mutex_wake();
  test_sleep(safety_timeout);
  assert(0);
}

void* cb_cancel_off(void* data) {
  pthread_cancel_on();
  pthread_cancel_off();
  test_mutex_wake();
  test_sleep(safety_timeout);
  return (void*) -159;
}

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

void* cb_async_on(void* data) {
  pthread_async_off();
  pthread_async_on();
  struct timespec time = {0};
  clock_gettime(CLOCK_REALTIME, &time);
  time.tv_sec += safety_timeout / 1000;
  time.tv_nsec += (safety_timeout % 1000) * 1000000;
  if(time.tv_nsec >= 1000000000) {
    time.tv_sec += time.tv_nsec / 1000000000;
    time.tv_nsec %= 1000000000;
  }
  test_mutex_wake();
  pthread_mutex_timedlock(&mutex, &time);
  assert(0);
}

void* cb_async_off(void* data) {
  pthread_async_on();
  pthread_async_off();
  struct timespec time = {0};
  clock_gettime(CLOCK_REALTIME, &time);
  time.tv_sec += safety_timeout / 1000;
  time.tv_nsec += (safety_timeout % 1000) * 1000000;
  if(time.tv_nsec >= 1000000000) {
    time.tv_sec += time.tv_nsec / 1000000000;
    time.tv_nsec %= 1000000000;
  }
  test_mutex_wake();
  assert(pthread_mutex_timedlock(&mutex, &time) == ETIMEDOUT);
  return (void*) 99;
}

test_register(int, pthread_create, (pthread_t* a, const pthread_attr_t* b, void* (*c)(void*), void* d), (a, b, c, d))

int main() {
  test_begin("thread check");
  test_error_check(int, pthread_create, ((void*) 0xbad, (void*) 0xbad, (void*) 0xbad, (void*) 0xbad));
  
  test_error_set_retval(pthread_create, ECANCELED);
  test_end();
  
  test_seed_random();
  pthread_mutex_lock(&mutex);
  
  pthread_t thread;
  
  test_begin("thread err");
  test_error(pthread_create);
  assert(pthread_start(&thread, assert_0, NULL));
  assert(test_error_get(pthread_create) == 0);
  test_end();
  
  test_begin("thread retval 1");
  assert(!pthread_start(&thread, cb_retval, (void*)-123));
  void* retval = NULL;
  pthread_join(thread, &retval);
  assert((uintptr_t) retval == (uintptr_t)-123);
  test_end();
  
  test_begin("thread retval 2");
  assert(!pthread_start(&thread, cb_wake, NULL));
  test_wait();
  pthread_cancel(thread);
  pthread_join(thread, &retval);
  assert(retval == PTHREAD_CANCELED);
  test_end();
  
  test_begin("thread stop");
  assert(!pthread_start(&thread, cb, NULL));
  pthread_cancel(thread);
  pthread_join(thread, NULL);
  test_end();
  
  test_begin("thread stop self");
  assert(!pthread_start(NULL, cb_stop_self, NULL));
  test_end();
  
  test_begin("thread stop sync");
  assert(!pthread_start(&thread, cb, NULL));
  pthread_cancel_sync(thread);
  test_end();
  
  test_begin("thread stop sync self");
  assert(!pthread_start(NULL, cb_stop_sync_self, NULL));
  test_end();
  
  test_begin("thread stop async");
  assert(!pthread_start(&thread, cb, NULL));
  pthread_cancel_async(thread);
  test_end();
  
  test_begin("thread stop async self");
  assert(!pthread_start(NULL, cb_stop_async_self, NULL));
  test_end();
  
  test_begin("thread cancel on");
  assert(!pthread_start(&thread, cb_cancel_on, NULL));
  test_mutex_wait();
  pthread_cancel(thread);
  retval = (void*) 0xbad;
  assert(!pthread_join(thread, &retval));
  assert(retval == PTHREAD_CANCELED);
  test_end();
  
  test_begin("thread cancel off");
  assert(!pthread_start(&thread, cb_cancel_off, NULL));
  test_mutex_wait();
  pthread_cancel(thread);
  retval = (void*) 0xbad;
  assert(!pthread_join(thread, &retval));
  assert(retval == (void*) -159);
  test_end();
  
  test_begin("thread async on");
  assert(!pthread_start(&thread, cb_async_on, NULL));
  test_mutex_wait();
  pthread_cancel(thread);
  retval = (void*) 0xbad;
  assert(!pthread_join(thread, &retval));
  assert(retval == PTHREAD_CANCELED);
  test_end();
  
  test_begin("thread async off");
  assert(!pthread_start(&thread, cb_async_off, NULL));
  test_mutex_wait();
  pthread_cancel(thread);
  retval = (void*) 0xbad;
  assert(!pthread_join(thread, &retval));
  assert(retval == (void*) 99);
  test_end();
  
  return 0;
}
