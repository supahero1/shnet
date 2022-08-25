#include "consts.h"

#include <time.h>
#include <errno.h>
#include <stdio.h>
#include <assert.h>
#include <signal.h>
#include <stdlib.h>
#include <inttypes.h>
#include <stdatomic.h>

#include <shnet/test.h>
#include <shnet/time.h>
#include <shnet/threads.h>

static _Atomic uint32_t counter;

static void posix_timer_callback(union sigval val) {
  (void) val;
  pthread_detach(pthread_self());
}

static void posix_timer_callback_all(union sigval val) {
  (void) val;
  pthread_detach(pthread_self());
  if(atomic_fetch_sub_explicit(&counter, 1, memory_order_relaxed) == 1) {
    test_wake();
  }
}

static void* thread_timer_callback_all(void* nil) {
  (void) nil;
  pthread_detach(pthread_self());
  if(atomic_fetch_sub_explicit(&counter, 1, memory_order_relaxed) == 1) {
    test_wake();
  }
  return NULL;
}

static void shnet_timer_callback(void* nil) {
  (void) nil;
}

static void shnet_timer_callback_all(void* nil) {
  (void) nil;
  if(atomic_fetch_sub_explicit(&counter, 1, memory_order_relaxed) == 1) {
    test_wake();
  }
}

#ifdef LIBUV

#include <uv.h>

static void libuv_timer_callback(uv_timer_t* handle) {
  (void) handle;
}

static void libuv_timer_callback_all(uv_timer_t* handle) {
  (void) handle;
  if(atomic_fetch_sub_explicit(&counter, 1, memory_order_relaxed) == 1) {
    test_wake();
  }
}

#endif

static uint64_t last_progress = UINT64_MAX;

#define PROGRESS(text) \
do { \
  const uint64_t progress = ((uint64_t) i * 1000) / options.num; \
  if(progress != last_progress) { \
    last_progress = progress; \
    print("\r" text " | %.1lf%%", (double) progress / 10.0); \
  } \
} while(0)

void time_benchmark() {
  const int default_num = 2000;
  if(options.num <= 0) {
    options.num = default_num;
  }
  print(
    "time-bench parameters:\n"
    "num : %" PRId32 "%s\n"
    "fast: %" PRIu32 "\n"
    "built with"
#ifndef LIBUV
    "out libuv. If you want to benchmark against it too, add \"WITH_LIBUV=1\"\n"
    "to your \"make\" command when building the library. Note that if you had already\n"
    "built the library, you need to clean it first with \"make clean\".\n"
#else
    " libuv\n"
#endif
    "\n",
    options.num,
    options.num == default_num ? " (the default)" : "",
    options.fast
  );
  DROP_IF_RIDICULOUS(options.num, "num", 0, 2048);

  uint64_t time = 0;

  if(!options.fast) {
    timer_t* const posix_timers = calloc(options.num, sizeof(timer_t));
    assert(posix_timers);

    for(int32_t i = 0; i < options.num; ++i) {
      PROGRESS("Initialising POSIX timers");
      const uint64_t start = time_get_time();
      int err = timer_create(CLOCK_REALTIME, &((struct sigevent) {
        .sigev_notify = SIGEV_THREAD,
        .sigev_value = (union sigval) {
          .sival_ptr = NULL
        },
        .sigev_notify_function = posix_timer_callback
      }), posix_timers + i);
      if(err == -1) {
        print("\ntimer_create(%" PRId32 ") failed with errno: %d\nConsider setting the \"num\" option to something below %d.\n", i, errno, options.num);
        assert(0);
      }
      err = timer_settime(posix_timers[i], 0, &((struct itimerspec) {
        .it_interval = (struct timespec) {
          .tv_sec = 0,
          .tv_nsec = 0
        },
        .it_value = (struct timespec) {
          .tv_sec = 999,
          .tv_nsec = 0
        }
      }), NULL);
      if(err == -1) {
        print("\ntimer_settime(%" PRId32 ") failed with errno: %d\n", i, errno);
        assert(0);
      }
      const uint64_t end = time_get_time();
      time += end - start;
    }

    print("\rInitialising POSIX timers took ");
    print_time(time);
    puts("");
    time = 0;

    for(int32_t i = 0; i < options.num; ++i) {
      PROGRESS("Deleting POSIX timers");
      const uint64_t start = time_get_time();
      int err = timer_delete(posix_timers[i]);
      if(err == -1) {
        print("\ntimer_delete(%" PRId32 ") failed with errno: %d\n", i, errno);
        assert(0);
      }
      const uint64_t end = time_get_time();
      time += end - start;
    }

    print("\rDeleting POSIX timers took ");
    print_time(time);
    puts("");
    time = 0;
    atomic_store(&counter, (uint32_t) options.num);

    for(int32_t i = 0; i < options.num; ++i) {
      PROGRESS("Initialising (and running) POSIX timers");
      const uint64_t start = time_get_time();
      int err = timer_create(CLOCK_REALTIME, &((struct sigevent) {
        .sigev_notify = SIGEV_THREAD,
        .sigev_value = (union sigval) {
          .sival_ptr = NULL
        },
        .sigev_notify_function = posix_timer_callback_all
      }), posix_timers + i);
      if(err == -1) {
        print("\ntimer_create(%" PRId32 ") failed with errno: %d\nConsider setting the \"num\" option to something below 63500.\n", i, errno);
        assert(0);
      }
      err = timer_settime(posix_timers[i], 0, &((struct itimerspec) {
        .it_interval = (struct timespec) {
          .tv_sec = 0,
          .tv_nsec = 0
        },
        .it_value = (struct timespec) {
          .tv_sec = 0,
          .tv_nsec = 1
        }
      }), NULL);
      if(err == -1) {
        print("\ntimer_settime(%" PRId32 ") failed with errno: %d\n", i, errno);
        assert(0);
      }
      const uint64_t end = time_get_time();
      time += end - start;
    }

    print("\rWaiting for all POSIX timers to run");
    {
      const uint64_t start = time_get_time();
      test_wait();
      const uint64_t end = time_get_time();
      time += end - start;
    }

    for(int32_t i = 0; i < options.num; ++i) {
      PROGRESS("Deleting POSIX timers");
      const uint64_t start = time_get_time();
      int err = timer_delete(posix_timers[i]);
      if(err == -1) {
        print("\ntimer_delete(%" PRId32 ") failed with errno: %d\n", i, errno);
        assert(0);
      }
      const uint64_t end = time_get_time();
      time += end - start;
    }

    print("\rInitialising, running, and deleting POSIX timers took ");
    print_time(time);
    puts("\n");
    time = 0;
    free(posix_timers);
    atomic_store(&counter, (uint32_t) options.num);

    /*
    * Initialisation of thread timers takes the same amount of time as
    * init + run + del, so it's useless to benchmark it. Besides, it
    * would also be significantly faster and better than in reality,
    * because the threads would just disappear almost instantly after
    * being created. No machine in reality can create millions of threads.
    * 
    * "Deleting" thread timers would mean some SIGNAL shenanigans to get
    * it to work really quickly. Not worth the effort here. Thread timers
    * eat more memory and are slower than the next 2 methods anyway.
    */

    for(int32_t i = 0; i < options.num; ++i) {
      PROGRESS("Initialising (and running) thread timers");
      const uint64_t start = time_get_time();
      int err = pthread_start(NULL, thread_timer_callback_all, NULL);
      if(err == -1) {
        print("\npthread_start(%" PRId32 ") failed with errno: %d\n", i, errno);
        assert(0);
      }
      const uint64_t end = time_get_time();
      time += end - start;
    }

    print("\rWaiting for all thread timers to run and delete themselves");
    {
      const uint64_t start = time_get_time();
      test_wait();
      const uint64_t end = time_get_time();
      time += end - start;
    }

    print("\rInitialising, running, and deleting thread timers took ");
    print_time(time);
    puts("\n");
    time = 0;
  }

  struct time_timers timers = {0};
  assert(!time_timers(&timers));
  assert(!time_start(&timers));

  struct time_timer* const shnet_timers = calloc(options.num, sizeof(struct time_timer));
  assert(shnet_timers);

  /* Not resizing the array of timers on purpose to make it more fair */
  for(int32_t i = 0; i < options.num; ++i) {
    PROGRESS("Initialising shnet timers");
    const uint64_t start = time_get_time();
    int err = time_add_timeout_raw(&timers, &((struct time_timeout) {
      .time = time_get_sec(999),
      .func = shnet_timer_callback,
      .ref = shnet_timers + i
    }));
    if(err == -1) {
      print("\ntime_add_timeout_raw(%" PRId32 ") failed with errno: %d\n", i, errno);
      assert(0);
    }
    const uint64_t end = time_get_time();
    time += end - start;
  }

  print("\rInitialising shnet timers took ");
  print_time(time);
  puts("");
  time = 0;
  time_lock(&timers);

  for(int32_t i = 0; i < options.num; ++i) {
    PROGRESS("Deleting shnet timers");
    const uint64_t start = time_get_time();
    /* This must not fail under any conditions */
    assert(!time_cancel_timeout_raw(&timers, shnet_timers + i));
    const uint64_t end = time_get_time();
    time += end - start;
  }

  time_unlock(&timers);
  print("\rDeleting shnet timers took ");
  print_time(time);
  puts("");
  time = 0;
  free(shnet_timers);
  atomic_store(&counter, (uint32_t) options.num);
  time_lock(&timers);

  for(int32_t i = 0; i < options.num; ++i) {
    PROGRESS("Initialising shnet timers");
    const uint64_t start = time_get_time();
    int err = time_add_timeout_raw(&timers, &((struct time_timeout) {
      .time = time_immediately,
      .func = shnet_timer_callback_all
    }));
    if(err == -1) {
      print("\ntime_add_timeout_raw(%" PRId32 ") failed with errno: %d\n", i, errno);
      assert(0);
    }
    const uint64_t end = time_get_time();
    time += end - start;
  }

  print("\rWaiting for all shnet timers to run and delete themselves");
  time_unlock(&timers);
  {
    const uint64_t start = time_get_time();
    test_wait();
    const uint64_t end = time_get_time();
    time += end - start;
  }

  print("\rInitialising, running, and deleting shnet timers took ");
  print_time(time);
#ifdef LIBUV
  puts("\n");
  time = 0;
#else
  puts("");
#endif
  time_stop_sync(&timers);
  time_free(&timers);

#ifdef LIBUV
  uv_loop_t* loop = uv_default_loop();
  assert(loop);

  uv_timer_t* const libuv_timers = calloc(options.num, sizeof(uv_timer_t));
  assert(libuv_timers);

  for(int32_t i = 0; i < options.num; ++i) {
    PROGRESS("Initialising libuv timers");
    const uint64_t start = time_get_time();
    int err = uv_timer_init(loop, libuv_timers + i);
    if(err != 0) {
      print("\nuv_timer_init(%" PRId32 ") failed with errno: %d\n", i, errno);
      assert(0);
    }
    err = uv_timer_start(libuv_timers + i, libuv_timer_callback, 999000, 0);
    if(err != 0) {
      print("\nuv_timer_start(%" PRId32 ") failed with errno: %d\n", i, errno);
      assert(0);
    }
    const uint64_t end = time_get_time();
    time += end - start;
  }

  print("\rInitialising libuv timers took ");
  print_time(time);
  puts("");
  time = 0;

  for(int32_t i = 0; i < options.num; ++i) {
    PROGRESS("Deleting libuv timers");
    const uint64_t start = time_get_time();
    int err = uv_timer_stop(libuv_timers + i);
    if(err != 0) {
      print("\nuv_timer_stop(%" PRId32 ") failed with errno: %d\n", i, errno);
      assert(0);
    }
    const uint64_t end = time_get_time();
    time += end - start;
  }

  time_unlock(&timers);
  print("\rDeleting libuv timers took ");
  print_time(time);
  puts("");
  time = 0;
  atomic_store(&counter, (uint32_t) options.num);

  for(int32_t i = 0; i < options.num; ++i) {
    PROGRESS("Initialising (and running) libuv timers");
    const uint64_t start = time_get_time();
    int err = uv_timer_init(loop, libuv_timers + i);
    if(err != 0) {
      print("\nuv_timer_init(%" PRId32 ") failed with errno: %d\n", i, errno);
      assert(0);
    }
    err = uv_timer_start(libuv_timers + i, libuv_timer_callback_all, 0, 0);
    if(err != 0) {
      print("\nuv_timer_start(%" PRId32 ") failed with errno: %d\n", i, errno);
      assert(0);
    }
    const uint64_t end = time_get_time();
    time += end - start;
  }

  print("\rWaiting for all libuv timers to run");
  {
    const uint64_t start = time_get_time();
    uv_run(loop, UV_RUN_DEFAULT);
    test_wait();
    const uint64_t end = time_get_time();
    time += end - start;
  }

  for(int32_t i = 0; i < options.num; ++i) {
    PROGRESS("Deleting libuv timers");
    const uint64_t start = time_get_time();
    int err = uv_timer_stop(libuv_timers + i);
    if(err != 0) {
      print("\nuv_timer_stop(%" PRId32 ") failed with errno: %d\n", i, errno);
      assert(0);
    }
    const uint64_t end = time_get_time();
    time += end - start;
  }

  print("\rInitialising, running, and deleting libuv timers took ");
  print_time(time);
  puts("");
  int err = uv_loop_close(loop);
  if(err != 0) {
    /* I really don't know how to fix this UV_EBUSY thing */
  }
  free(libuv_timers);
#endif
}
