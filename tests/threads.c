#include <shnet/tests.h>
#include <shnet/threads.h>

/* Set these to something like 25UL and 1
for Valgrind, otherwise it's gonna choke.

Try decreasing these 2 if you are
constantly assert(0)-ing below.

thread_count can't be less than 21. */

#define thread_count 35UL
#define repeat 15

void* assert_0(void* data) {
  assert(0);
}

void* cb(void* data) {
  test_sleep(100);
  assert(0);
}

void* cb2(void* data) {
  test_wake();
  test_sleep(100);
  assert(0);
}

void* quit(void* data) {
  return NULL;
}

void* quit_meaningful(void* data) {
  return data;
}

void* cancel_self(void* data) {
  pthread_cancel(pthread_self());
  test_sleep(100);
  assert(0);
}

void* sync_cancel_self(void* data) {
  pthread_cancel_sync(pthread_self());
  assert(0);
}

void* async_cancel_self(void* data) {
  pthread_cancel_async(pthread_self());
  test_sleep(100);
  assert(0);
}

void* cancellation_test(void* data) {
  pthread_cancel_off();
  test_wake();
  test_sleep(100);
  pthread_cancel_on();
  pthread_testcancel();
  assert(0);
}

test_register(int, pthread_create, (pthread_t* id, const pthread_attr_t* attr, void* (*func)(void*), void* data), (id, attr, func, data))

int main() {
  test_seed_random();
  
  pthread_t thread;
  pthreads_t threads = {0};
  
  begin_test("thread failure");
  test_error(pthread_create);
  assert(pthread_start(&thread, assert_0, NULL));
  assert(fail_pthread_create == -1);
  end_test();
  
  begin_test("thread 1");
  assert(!pthread_start(&thread, cb2, NULL));
  test_wait();
  pthread_cancel_sync(thread);
  end_test();
  
  begin_test("thread 2");
  assert(!pthread_start(&thread, quit, NULL));
  pthread_join(thread, NULL);
  end_test();
  
  begin_test("thread 3");
  assert(!pthread_start(&thread, quit_meaningful, (void*)-123));
  void* retval;
  pthread_join(thread, &retval);
  assert((uintptr_t) retval == (uintptr_t)-123);
  end_test();
  
  begin_test("thread 4");
  assert(!pthread_start(&thread, sync_cancel_self, NULL));
  test_sleep(100);
  end_test();
  
  begin_test("thread 5");
  assert(!pthread_start(&thread, cb, NULL));
  pthread_cancel_async(thread);
  test_sleep(100);
  end_test();
  
  begin_test("thread 6");
  assert(!pthread_start(&thread, async_cancel_self, NULL));
  test_sleep(100);
  end_test();
  
  begin_test("thread 7");
  assert(!pthread_start(&thread, cancel_self, NULL));
  pthread_join(thread, &retval);
  assert(retval == PTHREAD_CANCELED);
  end_test();
  
  begin_test("thread 8");
  assert(!pthread_start(&thread, cb2, NULL));
  test_wait();
  pthread_cancel(thread);
  pthread_join(thread, &retval);
  assert(retval == PTHREAD_CANCELED);
  end_test();
  
  begin_test("thread 9");
  assert(!pthread_start(&thread, cancellation_test, NULL));
  test_wait();
  pthread_cancel(thread);
  pthread_join(thread, &retval);
  assert(retval == PTHREAD_CANCELED);
  end_test();
  
  begin_test("thread 10");
  assert(!pthread_start(&thread, cancellation_test, NULL));
  test_wait();
  pthread_cancel_sync(thread);
  end_test();
  
  begin_test("thread 11");
  assert(!pthread_start(&thread, cancellation_test, NULL));
  test_wait();
  pthread_cancel_async(thread);
  test_sleep(100);
  end_test();
  
/* ========================================================================== */
  
  begin_test("threads thread");
  assert(!pthreads_start(&threads, cb, NULL, 1));
  assert(threads.used == 1);
  pthreads_shutdown_sync(&threads);
  assert(threads.used == 0);
  end_test();
  
  begin_test("threads failure 1");
  test_error_at(pthread_create, 4);
  assert(pthreads_start(&threads, assert_0, NULL, 5));
  assert(fail_pthread_create == -1);
  end_test();
  
  begin_test("threads failure 2");
  assert(!pthreads_start(&threads, cb, NULL, 5));
  test_error_at(pthread_create, 4);
  assert(pthreads_start(&threads, assert_0, NULL, 5));
  assert(fail_pthread_create == -1);
  assert(threads.used == 5);
  pthreads_shutdown_sync(&threads);
  assert(threads.used == 0);
  end_test();
  
  begin_test("threads sync 1");
  assert(!pthreads_resize(&threads, thread_count >> 1));
  assert(!pthreads_start(&threads, cb, NULL, thread_count));
  for(int i = 0; i < repeat; ++i) {
    pthreads_cancel_sync(&threads, 20 + (rand() % (threads.used - 19)));
    assert(!pthreads_start(&threads, cb, NULL, 20 + (rand() % (thread_count - threads.used))));
  }
  pthreads_shutdown_sync(&threads);
  end_test();
  
  begin_test("threads sync 2");
  for(int i = 0; i < repeat; ++i) {
    assert(!pthreads_start(&threads, cb, NULL, 20 + (rand() % (thread_count - threads.used))));
    pthreads_cancel_sync(&threads, 20 + (rand() % (threads.used - 19)));
  }
  pthreads_shutdown_sync(&threads);
  end_test();
  
  begin_test("threads async 1");
  assert(!pthreads_start(&threads, cb, NULL, thread_count));
  for(int i = 0; i < repeat; ++i) {
    pthreads_cancel_async(&threads, 20 + (rand() % (threads.used - 19)));
    assert(!pthreads_start(&threads, cb, NULL, 20 + (rand() % (thread_count - threads.used))));
  }
  pthreads_shutdown_async(&threads);
  end_test();
  
  begin_test("threads async 2");
  for(int i = 0; i < repeat; ++i) {
    assert(!pthreads_start(&threads, cb, NULL, 20 + (rand() % (thread_count - threads.used))));
    pthreads_cancel_async(&threads, 20 + (rand() % (threads.used - 19)));
  }
  pthreads_shutdown_async(&threads);
  end_test();
  
  begin_test("threads joinable 1");
  assert(!pthreads_start(&threads, cb, NULL, thread_count));
  for(int i = 0; i < repeat; ++i) {
    const uint32_t count = 20 + (rand() % (threads.used - 19));
    pthreads_cancel(&threads, count);
    for(uint32_t i = 0; i < count; ++i) {
      pthread_join(threads.ids[threads.used + i], &retval);
      assert(retval == PTHREAD_CANCELED);
    }
    assert(!pthreads_start(&threads, cb, NULL, 20 + (rand() % (thread_count - threads.used))));
  }
  {
    const uint32_t num = threads.used;
    pthreads_shutdown(&threads);
    for(uint32_t i = 0; i < num; ++i) {
      pthread_join(threads.ids[threads.used + i], &retval);
      assert(retval == PTHREAD_CANCELED);
    }
  }
  end_test();
  
  begin_test("threads joinable 2");
  for(int i = 0; i < repeat; ++i) {
    assert(!pthreads_start(&threads, cb, NULL, 20 + (rand() % (thread_count - threads.used))));
    const uint32_t count = 20 + (rand() % (threads.used - 19));
    pthreads_cancel(&threads, count);
    for(uint32_t i = 0; i < count; ++i) {
      pthread_join(threads.ids[threads.used + i], &retval);
      assert(retval == PTHREAD_CANCELED);
    }
  }
  {
    const uint32_t num = threads.used;
    pthreads_shutdown(&threads);
    for(uint32_t i = 0; i < num; ++i) {
      pthread_join(threads.ids[threads.used + i], &retval);
      assert(retval == PTHREAD_CANCELED);
    }
  }
  pthreads_free(&threads);
  end_test();
  
  return 0;
}