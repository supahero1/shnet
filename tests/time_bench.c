#include <shnet/tests.h>

#include <inttypes.h>
#include <stdatomic.h>

/* sudo make build-tests TESTLIBS="-DLIBUV -luv" */
#ifdef LIBUV
#include <uv.h>
#endif

#include <shnet/time.h>
#include <shnet/error.h>
#include <shnet/tests.h>
#include <shnet/threads.h>

#define TEST_NUM 5000

_Atomic unsigned long counter;

void native_timer_callback(union sigval val) {
  (void) val;
  pthread_detach(pthread_self());
}

void native_timer_callback_all(union sigval val) {
  if(atomic_fetch_sub_explicit(&counter, 1, memory_order_relaxed) == 1) {
    test_wake();
  }
}

void shnet_timer_callback(void* nil) {
  (void) nil;
}

void shnet_timer_callback_all(void* nil) {
  if(atomic_fetch_sub_explicit(&counter, 1, memory_order_relaxed) == 1) {
    test_wake();
  }
}

#ifdef LIBUV

void libuv_timer_callback(uv_timer_t* handle) {
  (void) handle;
}

void libuv_timer_callback_all(uv_timer_t* handle) {
  atomic_fetch_add_explicit(&counter, 1, memory_order_relaxed);
}

#endif

int main() {
  struct time_timers timers = {0};
  assert(!time_timers(&timers));
  assert(!time_timers_start(&timers));
  
  uint64_t start, end;
  timer_t native_timers[TEST_NUM];
  struct time_timer shnet_timers[TEST_NUM];
  
  begin_test("native timers creation");
  start = time_get_time();
  for(unsigned long i = 0; i < TEST_NUM; ++i) {
    assert(!timer_create(CLOCK_REALTIME, &((struct sigevent) {
      .sigev_notify = SIGEV_THREAD,
      .sigev_value = (union sigval) {
        .sival_ptr = NULL
      },
      .sigev_notify_function = native_timer_callback
    }), native_timers + i));
    assert(!timer_settime(native_timers[i], 0, &((struct itimerspec) {
      .it_interval = (struct timespec) {
        .tv_sec = 0,
        .tv_nsec = 0
      },
      .it_value = (struct timespec) {
        .tv_sec = 999,
        .tv_nsec = 0
      }
    }), NULL));
  }
  end = time_get_time();
  end_test();
  assert(printf("%" PRIu64 "us\n", time_ns_to_us(end - start)) > 0);
  
  begin_test("native timers deletion");
  start = time_get_time();
  for(unsigned long i = 0; i < TEST_NUM; ++i) {
    assert(!timer_delete(native_timers[i]));
  }
  end = time_get_time();
  end_test();
  assert(printf("%" PRIu64 "us\n", time_ns_to_us(end - start)) > 0);
  
  begin_test("native timers ALL");
  start = time_get_time();
  atomic_store(&counter, TEST_NUM);
  for(unsigned long i = 0; i < TEST_NUM; ++i) {
    assert(!timer_create(CLOCK_REALTIME, &((struct sigevent) {
      .sigev_notify = SIGEV_THREAD,
      .sigev_value = (union sigval) {
        .sival_ptr = NULL
      },
      .sigev_notify_function = native_timer_callback_all
    }), native_timers + i));
    assert(!timer_settime(native_timers[i], 0, &((struct itimerspec) {
      .it_interval = (struct timespec) {
        .tv_sec = 0,
        .tv_nsec = 0
      },
      .it_value = (struct timespec) {
        .tv_sec = 0,
        .tv_nsec = 1
      }
    }), NULL));
  }
  test_wait();
  for(unsigned long i = 0; i < TEST_NUM; ++i) {
    assert(!timer_delete(native_timers[i]));
  }
  end = time_get_time();
  end_test();
  assert(printf("%" PRIu64 "us\n", time_ns_to_us(end - start)) > 0);
  
  begin_test("shnet timers creation");
  start = time_get_time();
  time_lock(&timers);
  for(unsigned long i = 0; i < TEST_NUM; ++i) {
    assert(!time_add_timeout_raw(&timers, &((struct time_timeout) {
      .time = time_get_sec(999),
      .func = shnet_timer_callback,
      .ref = shnet_timers + i
    })));
  }
  time_unlock(&timers);
  end = time_get_time();
  end_test();
  assert(printf("%" PRIu64 "us\n", time_ns_to_us(end - start)) > 0);
  
  begin_test("shnet timers deletion");
  start = time_get_time();
  time_lock(&timers);
  for(unsigned long i = 0; i < TEST_NUM; ++i) {
    assert(!time_cancel_timeout_raw(&timers, shnet_timers + i));
  }
  time_unlock(&timers);
  end = time_get_time();
  end_test();
  assert(printf("%" PRIu64 "us\n", time_ns_to_us(end - start)) > 0);
  
  begin_test("shnet timers ALL");
  start = time_get_time();
  atomic_store(&counter, TEST_NUM);
  time_lock(&timers);
  for(unsigned long i = 0; i < TEST_NUM; ++i) {
    assert(!time_add_timeout_raw(&timers, &((struct time_timeout) {
      .time = TIME_IMMEDIATELY,
      .func = shnet_timer_callback_all
    })));
  }
  time_unlock(&timers);
  test_wait();
  end = time_get_time();
  end_test();
  assert(printf("%" PRIu64 "us\n", time_ns_to_us(end - start)) > 0);
  time_timers_stop(&timers);
  time_timers_free(&timers);
  
#ifdef LIBUV
  uv_loop_t* loop = uv_default_loop();
  assert(loop);
  uv_timer_t uv_timers[TEST_NUM] = {0};
  
  begin_test("libuv timers creation");
  start = time_get_time();
  for(unsigned int i = 0; i < TEST_NUM; ++i) {
    assert(!uv_timer_init(loop, uv_timers + i));
    assert(!uv_timer_start(uv_timers + i, libuv_timer_callback, 999000, 0));
  }
  end = time_get_time();
  end_test();
  assert(printf("%" PRIu64 "us\n", time_ns_to_us(end - start)) > 0);
  
  begin_test("libuv timers deletion");
  start = time_get_time();
  for(unsigned int i = 0; i < TEST_NUM; ++i) {
    assert(!uv_timer_stop(uv_timers + i));
  }
  end = time_get_time();
  end_test();
  assert(printf("%" PRIu64 "us\n", time_ns_to_us(end - start)) > 0);
  
  begin_test("libuv timers ALL");
  start = time_get_time();
  atomic_store(&counter, 0);
  for(unsigned int i = 0; i < TEST_NUM; ++i) {
    assert(!uv_timer_init(loop, uv_timers + i));
    assert(!uv_timer_start(uv_timers + i, libuv_timer_callback_all, 0, 0));
  }
  uv_run(loop, UV_RUN_DEFAULT);
  end = time_get_time();
  end_test();
  assert(printf("%" PRIu64 "us\n", time_ns_to_us(end - start)) > 0);
  assert(atomic_load(&counter) == TEST_NUM);
  for(unsigned int i = 0; i < TEST_NUM; ++i) {
    assert(!uv_timer_stop(uv_timers + i));
  }
  /* Always returns UV_EBUSY for some reason, mem leak */
  uv_loop_close(loop);
#endif
  
  return 0;
}