#include <shnet/tests.h>

/*
 * NOTE: these tests MAY fail if
 * the system is under heavy load.
 */

#include <math.h>
#include <stdatomic.h>

#include <shnet/time.h>

_Atomic uint32_t counter;
_Atomic uint32_t total_size;
struct time_timers timers = {0};
int no = 0;
uint64_t last_frame;
uint64_t frames[100];
int frames_len = 0;

struct test4_data {
  struct time_timer interval;
  struct time_timer timeout;
};

void count_up(void* data) {
  const uint32_t now = atomic_fetch_add_explicit(&counter, 1, memory_order_relaxed);
  const uint32_t total = atomic_load(&total_size);
  if(now + 1 == total) {
    test_wake();
  }
}

void cancel_timeout(void* data) {
  assert(!time_cancel_timeout(&timers, data));
}

void assert_0(void* data) {
  assert(0);
}

void cancel_interval(void* data) {
  assert(!time_cancel_interval(&timers, data));
}

void cancel_timeout_self(void* data) {
  assert(time_cancel_timeout(&timers, data));
  assert(time_open_timeout(&timers, data) == NULL);
}

void cancel_interval_self(void* data) {
  assert(time_open_interval(&timers, data) != NULL);
  time_close_interval(&timers, data);
  assert(!time_cancel_interval(&timers, data));
  assert(time_open_interval(&timers, data) == NULL);
}

void cancel_timeout_and_die(void* data) {
  struct test4_data* d = (struct test4_data*) data;
  assert(!no);
  no = 1;
  time_lock(&timers);
  time_cancel_timeout_raw(&timers, &d->timeout);
  time_cancel_interval_raw(&timers, &d->interval);
  time_unlock(&timers);
}

void cancel_interval_and_die(void* data) {
  struct test4_data* d = (struct test4_data*) data;
  assert(!no);
  no = 1;
  time_lock(&timers);
  time_cancel_interval_raw(&timers, &d->timeout);
  time_cancel_interval_raw(&timers, &d->interval);
  time_unlock(&timers);
}

void frame(void* data) {
  uint64_t now = time_get_time();
  frames[frames_len++] = now - last_frame;
  last_frame = now;
  if(frames_len == sizeof(frames) / sizeof(frames[0])) {
    time_cancel_interval(&timers, data);
    test_wake();
  }
}

void test7_modify_timeout(void* data) {
  struct time_timeout* const timeout = time_open_timeout(&timers, data);
  assert(timeout != NULL);
  timeout->time = TIME_IMMEDIATELY;
  time_close_timeout(&timers, data);
}

void close_timers(void* nil) {
  time_timers_stop_joinable(&timers);
}

int main() {
  atomic_store(&counter, 0);
  atomic_store(&total_size, 0);
  test_seed_random();
  
  begin_test("time init");
  assert(!time_timers(&timers));
  assert(!time_timers_start(&timers));
  assert(!time_resize_timeouts(&timers, timers.timeouts_used + 100));
  
  struct time_timer timer;
  for(int i = 0; i < 100; ++i) {
    assert(!time_add_timeout(&timers, &((struct time_timeout) {
      .func = count_up,
      .time = time_ms_to_ns(100) + time_get_ms(rand() % 100),
      .ref = &timer
    })));
    if(i < (rand() % 50)) {
      time_cancel_timeout(&timers, &timer);
    } else {
      atomic_fetch_add(&total_size, 1);
    }
  }
  assert(timers.timeouts_used == atomic_load(&total_size) + 1);
  test_wait();
  assert(!time_resize_timeouts(&timers, 1));
  end_test();
  
  begin_test("time timeout cancellation");
  assert(!time_add_timeout(&timers, &((struct time_timeout) {
    .func = assert_0,
    .time = time_ms_to_ns(100),
    .ref = &timer
  })));
  assert(!time_add_timeout(&timers, &((struct time_timeout) {
    .func = cancel_timeout,
    .data = &timer,
    .time = time_ms_to_ns(50)
  })));
  usleep(time_ms_to_us(150));
  end_test();
  
  begin_test("time interval cancellation");
  assert(!time_add_interval(&timers, &((struct time_interval) {
    .func = assert_0,
    .base_time = time_get_ms(100),
    .ref = &timer
  })));
  assert(!time_add_timeout(&timers, &((struct time_timeout) {
    .func = cancel_interval,
    .data = &timer,
    .time = time_get_ms(50)
  })));
  usleep(time_ms_to_us(150));
  end_test();
  
  begin_test("time cancellation 1");
  no = 0;
  struct test4_data test4_data;
  assert(!time_add_timeout(&timers, &((struct time_timeout) {
    .func = assert_0,
    .time = time_get_ms(100),
    .ref = &test4_data.timeout
  })));
  assert(!time_add_interval(&timers, &((struct time_interval) {
    .func = cancel_timeout_and_die,
    .data = &test4_data,
    .base_time = time_get_time(),
    .interval = time_ms_to_ns(1),
    .count = 50,
    .ref = &test4_data.interval
  })));
  usleep(time_ms_to_us(150));
  end_test();
  
  begin_test("time cancellation 2");
  no = 0;
  assert(!time_add_interval(&timers, &((struct time_interval) {
    .func = assert_0,
    .base_time = time_get_ms(100),
    .ref = &test4_data.timeout
  })));
  assert(!time_add_interval(&timers, &((struct time_interval) {
    .func = cancel_interval_and_die,
    .data = &test4_data,
    .base_time = time_get_ms(50),
    .ref = &test4_data.interval
  })));
  usleep(time_ms_to_us(150));
  end_test();
  
  begin_test("time benchmark");
  assert(!time_add_interval(&timers, &((struct time_interval) {
    .func = frame,
    .data = &timer,
    .ref = &timer,
    .base_time = time_get_ns(16666666),
    .interval = 16666666
  })));
  last_frame = time_get_time();
  test_wait();
  double min = UINT32_MAX;
  double max = 0;
  double avg = 0;
  for(int i = 0; i < frames_len; ++i) {
    avg += frames[i];
    if(frames[i] > max) {
      max = frames[i];
    }
    if(frames[i] < min) {
      min = frames[i];
    }
  }
  min /= time_ms_to_ns(1);
  max /= time_ms_to_ns(1);
  avg /= frames_len * time_ms_to_ns(1);
  double dev = 0;
  for(int i = 0; i < frames_len; ++i) {
    dev += ((double)(frames[i]/time_ms_to_ns(1)) - avg) * ((double)(frames[i]/time_ms_to_ns(1)) - avg);
  }
  dev = sqrt(dev / frames_len);
  end_test();
  printf("Dev stats\nmin: %.2lf\nmax: %.2lf\navg: %.2lf\nsde: %.2lf\n", min, max, avg, dev);
  
  begin_test("time timer modification");
  assert(!time_add_timeout(&timers, &((struct time_timeout) {
    .func = assert_0,
    .time = time_get_ms(100),
    .ref = &timer
  })));
  struct time_timer timer2;
  assert(!time_add_timeout(&timers, &((struct time_timeout) {
    .func = cancel_timeout,
    .time = time_get_ms(200),
    .data = &timer,
    .ref = &timer2
  })));
  assert(!time_add_timeout(&timers, &((struct time_timeout) {
    .func = test7_modify_timeout,
    .time = time_get_ms(50),
    .data = &timer2
  })));
  usleep(time_ms_to_us(150));
  end_test();
  
  begin_test("time timeout self cancel");
  assert(!time_add_timeout(&timers, &((struct time_timeout) {
    .func = cancel_timeout_self,
    .time = TIME_IMMEDIATELY,
    .data = &timer,
    .ref = &timer
  })));
  usleep(time_ms_to_us(50));
  end_test();
  
  begin_test("time interval self cancel");
  assert(!time_add_interval(&timers, &((struct time_interval) {
    .func = cancel_interval_self,
    .base_time = time_get_time(),
    .data = &timer,
    .ref = &timer
  })));
  usleep(time_ms_to_us(50));
  end_test();
  
  begin_test("time cleanup");
  assert(!time_add_timeout(&timers, &((struct time_timeout) {
    .time = TIME_IMMEDIATELY,
    .func = close_timers
  })));
  (void) pthread_join(timers.thread, NULL);
  end_test();
  
  time_timers_free(&timers);
  return 0;
}