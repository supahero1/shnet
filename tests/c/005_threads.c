#include <shnet/test.h>

#include <stdlib.h>
#include <string.h>

#include <shnet/threads.h>

#ifndef SHNET_TEST_VALGRIND

#define thread_count 50UL
#define repeat 20

#define safety_timeout 500

#else

#define thread_count 25UL
#define repeat 3

#define safety_timeout 30000

#endif // SHNET_TEST_VALGRIND

struct test_info {
  int del_num;
  pthreads_t* threads;
};

void* assert_0(void* data) {
  assert(0);
}

void* cb(void* data) {
  test_sleep(safety_timeout);
  assert(0);
}

void* cb_stop_self(void* data) {
  struct test_info* test = (struct test_info*) data;
  test_wait();
  pthread_detach(pthread_self());
  pthreads_cancel(test->threads, test->del_num);
  test_sleep(safety_timeout);
  assert(0);
}

void cb_commit(void* nil) {
  test_wake();
}

void* cb_stop_sync_self(void* data) {
  pthread_cleanup_push(cb_commit, NULL);
  pthreads_t* threads = (pthreads_t*) data;
  pthreads_cancel_sync(threads, 20 + ((threads->used != 19) ? (rand() % (threads->used - 19)) : 0));
  assert(0);
  pthread_cleanup_pop(1);
}

void* cb_stop_async_self(void* data) {
  pthread_cleanup_push(cb_commit, NULL);
  pthreads_t* threads = (pthreads_t*) data;
  pthreads_cancel_async(threads, 20 + ((threads->used != 19) ? (rand() % (threads->used - 19)) : 0));
  test_sleep(safety_timeout);
  assert(0);
  pthread_cleanup_pop(1);
}

test_register(void*, shnet_malloc, (const size_t a), (a))
test_register(void*, shnet_realloc, (void* const a, const size_t b), (a, b))
test_register(int, pthread_create, (pthread_t* a, const pthread_attr_t* b, void* (*c)(void*), void* d), (a, b, c, d))
test_register(int, sem_init, (sem_t* a, int b, unsigned int c), (a, b, c))
test_register(int, pthread_mutex_init, (pthread_mutex_t* restrict a, const pthread_mutexattr_t* restrict b), (a, b))

int main() {
  test_begin("threads check");
  test_error_check(void*, shnet_malloc, (0xbad));
  test_error_check(void*, shnet_realloc, ((void*) 0xbad, 0xbad));
  test_error_check(int, pthread_create, ((void*) 0xbad, (void*) 0xbad, (void*) 0xbad, (void*) 0xbad));
  test_error_check(int, sem_init, ((void*) 0xbad, 0xbad, 0xbad));
  test_error_check(int, pthread_mutex_init, ((void*) 0x1bad, (void*) 0x2bad));
  
  test_error_set_retval(shnet_malloc, NULL);
  test_error_set_retval(shnet_realloc, NULL);
  test_error_set_retval(pthread_mutex_init, ENOMEM);
  test_end();
  
  test_seed_random();
  
  pthreads_t threads = {0};
  pthreads_t tem;
  void* retval = (void*) -123;
  
  test_begin("threads thread");
  assert(!pthreads_start(&threads, cb, NULL, 1));
  assert(threads.used == 1);
  pthreads_shutdown_sync(&threads);
  assert(threads.used == 0);
  test_end();
  
  test_begin("threads err 1");
  test_error(shnet_realloc);
  tem = threads;
  assert(pthreads_resize(&threads, 0xbad) == -1);
  assert(memcmp(&threads, &tem, sizeof(threads)) == 0);
  test_end();
  
  test_begin("threads err 2");
  test_error_set(pthread_create, 4);
  tem = threads;
  assert(pthreads_start(&threads, assert_0, NULL, 5));
  assert(threads.used == 0);
  assert(threads.size == 5);
  test_end();
  
  test_begin("threads err 3");
  assert(!pthreads_start(&threads, cb, NULL, 5));
  test_error_set(pthread_create, 4);
  tem = threads;
  assert(pthreads_start(&threads, assert_0, NULL, 5));
  assert(threads.used == 5);
  assert(threads.size == 10);
  pthreads_shutdown_sync(&threads);
  test_end();
  
  test_begin("threads resize 0");
  assert(threads.used == 0);
  assert(threads.size == 10);
  assert(!pthreads_resize(&threads, 0));
  assert(threads.used == 0);
  assert(threads.size == 0);
  assert(threads.ids == NULL);
  test_end();
  
  test_begin("threads resize");
  assert(!pthreads_resize(&threads, threads.size));
  test_end();
  
  test_begin("threads err 4");
  test_error(shnet_malloc);
  assert(pthreads_start(&threads, assert_0, NULL, 5));
  assert(threads.used == 0);
  assert(threads.size == 5);
  assert(threads.ids != NULL);
  test_end();
  
  test_begin("threads err 5");
  test_error(sem_init);
  assert(pthreads_start(&threads, assert_0, NULL, 5));
  assert(threads.used == 0);
  assert(threads.size == 5);
  assert(threads.ids != NULL);
  test_end();
  
  test_begin("threads err 6");
  test_error(pthread_mutex_init);
  assert(pthreads_start(&threads, assert_0, NULL, 5));
  assert(threads.used == 0);
  assert(threads.size == 5);
  assert(threads.ids != NULL);
  test_end();
  
  test_begin("threads stop");
  for(int i = 0; i < repeat; ++i) {
    assert(!pthreads_start(&threads, cb, NULL, 20 + ((threads.used != thread_count) ? (rand() % (thread_count - threads.used)) : 0)));
    int num = 20 + ((threads.used != 19) ? (rand() % (threads.used - 19)) : 0);
    for(int j = 1; j <= num; ++j) {
      (void) pthread_detach(threads.ids[threads.used - j]);
    }
    pthreads_cancel(&threads, num);
  }
  pthreads_shutdown_sync(&threads);
  test_end();
  
  test_begin("threads stop self");
  for(int i = 0; i < repeat; ++i) {
    assert(!pthreads_start(&threads, cb, NULL, 20 + ((threads.used != thread_count) ? (rand() % (thread_count - threads.used)) : 0)));
    int num = 20 + ((threads.used != 19) ? (rand() % (threads.used - 19)) : 0);
    struct test_info test = (struct test_info) {
      .del_num = num,
      .threads = &threads
    };
    assert(!pthreads_start(&threads, cb_stop_self, &test, 1));
    retval = (void*) -123;
    pthread_t id = threads.ids[threads.used - 1];
    test_wake();
    assert(!pthread_join(id, &retval));
    assert(retval == PTHREAD_CANCELED);
    for(int j = 0; j < num - 1; ++j) {
      retval = (void*) -123;
      assert(!pthread_join(threads.ids[threads.used + j], &retval));
      assert(retval == PTHREAD_CANCELED);
    }
  }
  pthreads_shutdown_sync(&threads);
  test_end();
  
  test_begin("threads stop sync");
  for(int i = 0; i < repeat; ++i) {
    assert(!pthreads_start(&threads, cb, NULL, 20 + ((threads.used != thread_count) ? (rand() % (thread_count - threads.used)) : 0)));
    pthreads_cancel_sync(&threads, 20 + ((threads.used != 19) ? (rand() % (threads.used - 19)) : 0));
  }
  pthreads_shutdown_sync(&threads);
  test_end();
  
  test_begin("threads stop sync self");
  for(int i = 0; i < repeat; ++i) {
    assert(!pthreads_start(&threads, cb, NULL, 20 + ((threads.used != thread_count) ? (rand() % (thread_count - threads.used)) : 0)));
    assert(!pthreads_start(&threads, cb_stop_sync_self, &threads, 1));
    test_wait();
  }
  pthreads_shutdown_sync(&threads);
  test_end();
  
  test_begin("threads stop async");
  for(int i = 0; i < repeat; ++i) {
    assert(!pthreads_start(&threads, cb, NULL, 20 + ((threads.used != thread_count) ? (rand() % (thread_count - threads.used)) : 0)));
    pthreads_cancel_async(&threads, 20 + ((threads.used != 19) ? (rand() % (threads.used - 19)) : 0));
  }
  pthreads_shutdown_sync(&threads);
  test_end();
  
  test_begin("threads stop async self");
  for(int i = 0; i < repeat; ++i) {
    assert(!pthreads_start(&threads, cb, NULL, 20 + ((threads.used != thread_count) ? (rand() % (thread_count - threads.used)) : 0)));
    assert(!pthreads_start(&threads, cb_stop_async_self, &threads, 1));
    test_wait();
  }
  pthreads_shutdown_sync(&threads);
  test_end();
  
  test_begin("threads stress sync 1");
  assert(!pthreads_start(&threads, cb, NULL, thread_count));
  for(int i = 0; i < repeat; ++i) {
    pthreads_cancel_sync(&threads, 20 + ((threads.used != 19) ? (rand() % (threads.used - 19)) : 0));
    assert(!pthreads_start(&threads, cb, NULL, 20 + ((threads.used != thread_count) ? (rand() % (thread_count - threads.used)) : 0)));
  }
  pthreads_shutdown_sync(&threads);
  test_end();
  
  test_begin("threads stress sync 2");
  for(int i = 0; i < repeat; ++i) {
    assert(!pthreads_start(&threads, cb, NULL, 20 + ((threads.used != thread_count) ? (rand() % (thread_count - threads.used)) : 0)));
    pthreads_cancel_sync(&threads, 20 + ((threads.used != 19) ? (rand() % (threads.used - 19)) : 0));
  }
  pthreads_shutdown_sync(&threads);
  test_end();
  
  test_begin("threads stress async 1");
  assert(!pthreads_start(&threads, cb, NULL, thread_count));
  for(int i = 0; i < repeat; ++i) {
    pthreads_cancel_async(&threads, 20 + ((threads.used != 19) ? (rand() % (threads.used - 19)) : 0));
    assert(!pthreads_start(&threads, cb, NULL, 20 + ((threads.used != thread_count) ? (rand() % (thread_count - threads.used)) : 0)));
  }
  pthreads_shutdown_async(&threads);
  test_end();
  
  test_begin("threads stress async 2");
  for(int i = 0; i < repeat; ++i) {
    assert(!pthreads_start(&threads, cb, NULL, 20 + ((threads.used != thread_count) ? (rand() % (thread_count - threads.used)) : 0)));
    pthreads_cancel_async(&threads, 20 + ((threads.used != 19) ? (rand() % (threads.used - 19)) : 0));
  }
  pthreads_shutdown_async(&threads);
  test_end();
  
  test_begin("threads stress joinable 1");
  assert(!pthreads_start(&threads, cb, NULL, thread_count));
  for(int i = 0; i < repeat; ++i) {
    const uint32_t count = 20 + ((threads.used != 19) ? (rand() % (threads.used - 19)) : 0);
    pthreads_cancel(&threads, count);
    for(uint32_t i = 0; i < count; ++i) {
      retval = (void*) -123;
      assert(!pthread_join(threads.ids[threads.used + i], &retval));
      assert(retval == PTHREAD_CANCELED);
    }
    assert(!pthreads_start(&threads, cb, NULL, 20 + ((threads.used != thread_count) ? (rand() % (thread_count - threads.used)) : 0)));
  }
  {
    const uint32_t num = threads.used;
    pthreads_shutdown(&threads);
    for(uint32_t i = 0; i < num; ++i) {
      retval = (void*) -123;
      assert(!pthread_join(threads.ids[threads.used + i], &retval));
      assert(retval == PTHREAD_CANCELED);
    }
  }
  test_end();
  
  test_begin("threads stress joinable 2");
  for(int i = 0; i < repeat; ++i) {
    assert(!pthreads_start(&threads, cb, NULL, 20 + ((threads.used != thread_count) ? (rand() % (thread_count - threads.used)) : 0)));
    const uint32_t count = 20 + ((threads.used != 19) ? (rand() % (threads.used - 19)) : 0);
    pthreads_cancel(&threads, count);
    for(uint32_t i = 0; i < count; ++i) {
      retval = (void*) -123;
      assert(!pthread_join(threads.ids[threads.used + i], &retval));
      assert(retval == PTHREAD_CANCELED);
    }
  }
  {
    const uint32_t num = threads.used;
    pthreads_shutdown(&threads);
    for(uint32_t i = 0; i < num; ++i) {
      retval = (void*) -123;
      assert(!pthread_join(threads.ids[threads.used + i], &retval));
      assert(retval == PTHREAD_CANCELED);
    }
  }
  pthreads_free(&threads);
  test_end();
  
  return 0;
}
