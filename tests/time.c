#include "tests.h"

#include <math.h>
#include <time.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <shnet/time.h>

void on_timeout_expire(struct time_manager* manager, void* lool) {
  struct time_manager_node* const timeout = (struct time_manager_node*) lool;
  timeout->func(timeout->data);
}

_Atomic unsigned char last;

void cbfunc4(void*);

void cbfunc1(void* data) {
  (void) data;
  if(atomic_load(&last) != 2) {
    TEST_FAIL;
  }
  atomic_store(&last, 0);
  (void) time_manager_add_timer(data, time_get_ms(500), cbfunc4, data, 1);
  if(errno != time_success) {
    TEST_FAIL;
  }
}

void cbfunc2(void* data) {
  (void) data;
  if(atomic_load(&last) != 1) {
    TEST_FAIL;
  }
  atomic_store(&last, 2);
}

void cbfunc3(void* data) {
  (void) data;
  if(atomic_load(&last) != 0) {
    TEST_FAIL;
  }
  atomic_store(&last, 1);
}

void cbfunc5(void*);

void cbfunc6(void*);

void cbfunc4(void* data) {
  atomic_fetch_add(&last, 1);
  if(atomic_load(&last) == 1) {
    for(int i = 0; i < 11; ++i) {
      (void) time_manager_add_timer(data, time_get_ms(500), cbfunc4, data, 1);
      if(errno != time_success) {
        TEST_FAIL;
      }
    }
    (void) time_manager_add_timer(data, time_get_sec(1), cbfunc5, data, 1);
    if(errno != time_success) {
      TEST_FAIL;
    }
    uint64_t time = time_get_ms(750);
    unsigned long id = time_manager_add_timer(data, time, cbfunc6, data, 1);
    if(errno != time_success) {
      TEST_FAIL;
    }
    time_manager_cancel_timer(data, time, id, 1);
    time = time_get_ms(1);
    id = time_manager_add_timer(data, time, cbfunc6, data, 1);
    if(errno != time_success) {
      TEST_FAIL;
    }
    time_manager_cancel_timer(data, time, id, 1);
  }
}

void cbfunc5(void* data) {
  (void) data;
  if(atomic_load(&last) != 12) {
    TEST_FAIL;
  }
  time_manager_stop(data);
}

void cbfunc6(void* data) {
  TEST_FAIL;
}

uint64_t begin;
uint64_t times;
uint64_t last_time;
uint64_t min = 100000000000;
uint64_t max = 0;
uint64_t avg[1000];

void cbbenchfunc(void* data) {
  uint64_t now = time_get_ns(0);
  uint64_t diff = now - last_time;
  if(min > diff) {
    min = diff;
  }
  if(max < diff) {
    max = diff;
  }
  avg[times - 1] = diff;
  printf("\r%.1f%%", (float) times / 10);
  fflush(stdout);
  if(times == 1000) {
    float average = 0;
    for(int i = 0; i < 1000; ++i) {
      average += (float) avg[i] / 1000000;
    }
    average /= 1000;
    float sd = 0;
    for(int i = 0; i < 1000; ++i) {
      sd += ((float) avg[i] / 1000000 - average) * ((float) avg[i] / 1000000 - average);
    }
    sd /= 1000;
    sd = sqrt(sd);
    printf_debug("\rAverage: %.2f ms\nMin: %.2f ms\nMax: %.2f ms\nStandard deviation: %.2f", 1, average, (float) min / 1000000, (float) max / 1000000, sd);
    TEST_PASS;
    exit(0);
  }
  last_time = now;
  (void) time_manager_add_timer(data, begin + 16666666 * times++, cbbenchfunc, data, 1);
  if(errno != time_success) {
    TEST_FAIL;
  }
}

int counter = 0;

int main() {
  printf_debug("Testing time.c:", 1);
  printf_debug("1. Stress test", 1);
  puts("If a round takes longer than 5 seconds, consider the test failed and feel free to break out using an interrupt (^C).");
  {
    struct timespec tp;
    (void) clock_gettime(CLOCK_REALTIME, &tp);
    srand(tp.tv_nsec + tp.tv_sec * 1000000000);
  }
  struct time_manager manager;
  start:
  printf("\rRound %d/3", counter + 1);
  fflush(stdout);
  int err = time_manager(&manager, on_timeout_expire, 1, 0);
  if(err != time_success) {
    TEST_FAIL;
  }
  manager.on_start = NULL;
  manager.on_stop = NULL;
  err = time_manager_start(&manager);
  if(err != time_success) {
    TEST_FAIL;
  }
  atomic_store(&last, 0);
  (void) time_manager_add_timer(&manager, time_get_ms(1500), cbfunc1, &manager, 0);
  if(errno != time_success) {
    TEST_FAIL;
  }
  (void) time_manager_add_timer(&manager, time_get_ms(500), cbfunc3, &manager, 0);
  if(errno != time_success) {
    TEST_FAIL;
  }
  (void) time_manager_add_timer(&manager, time_get_sec(1), cbfunc2, &manager, 0);
  if(errno != time_success) {
    TEST_FAIL;
  }
  (void) pthread_join(manager.worker, NULL);
  if(counter++ == 2) {
    puts("");
    TEST_PASS;
    goto benchmark;
  }
  goto start;
  benchmark:
  printf_debug("2. Benchmark", 1);
  err = time_manager(&manager, on_timeout_expire, 1, 0);
  if(err != time_success) {
    TEST_FAIL;
  }
  manager.on_start = NULL;
  manager.on_stop = NULL;
  err = time_manager_start(&manager);
  if(err != time_success) {
    TEST_FAIL;
  }
  begin = time_get_ns(16666666);
  last_time = begin - 16666666;
  times = 1;
  (void) time_manager_add_timer(&manager, begin, cbbenchfunc, &manager, 0);
  if(errno != time_success) {
    TEST_FAIL;
  }
  getc(stdin);
  return 0;
}