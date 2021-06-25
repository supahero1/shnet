#include "tests.h"

#include <math.h>
#include <time.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <shnet/time.h>

_Atomic unsigned char last;

pthread_mutex_t mutexo = PTHREAD_MUTEX_INITIALIZER;

void cbfunc4(void*);

void cbfunc1(void* data) {
  (void) data;
  if(atomic_load(&last) != 2) {
    TEST_FAIL;
  }
  atomic_store(&last, 0);
  int err = time_manager_add_timeout(data, time_get_ms(500), cbfunc4, data, NULL);
  if(err != time_success) {
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
  int err;
  if(atomic_fetch_add(&last, 1) == 0) {
    for(int i = 0; i < 11; ++i) {
      err = time_manager_add_timeout(data, time_get_ms(500), cbfunc4, data, NULL);
      if(err != time_success) {
        TEST_FAIL;
      }
    }
    err = time_manager_add_timeout(data, time_get_sec(1), cbfunc5, data, NULL);
    if(err != time_success) {
      TEST_FAIL;
    }
    struct time_timeout* timeout;
    struct time_timeout* timeout2;
    struct time_interval* interval;
    err = time_manager_add_timeout(data, time_get_ms(750), cbfunc6, data, &timeout);
    if(err != time_success) {
      TEST_FAIL;
    }
    err = time_manager_add_interval(data, time_get_ms(100), time_ms_to_ns(100), cbfunc6, data, &interval, time_not_instant);
    if(err != time_success) {
      TEST_FAIL;
    }
    err = time_manager_add_timeout(data, time_get_ms(5), cbfunc6, data, &timeout2);
    if(err != time_success) {
      TEST_FAIL;
    }
    time_manager_cancel_timeout(data, timeout);
    time_manager_cancel_timeout(data, timeout2);
    time_manager_cancel_interval(data, interval);
  }
}

void cbfunc5(void* data) {
  (void) data;
  if(atomic_load(&last) != 12) {
    TEST_FAIL;
  }
  time_manager_stop(data);
  (void) pthread_mutex_unlock(&mutexo);
  pthread_exit(NULL);
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

int canQuit = 0;

void cbbenchfunc(void* data) {
  uint64_t now = time_get_time();
  uint64_t diff = now - last_time;
  last_time = now;
  if(min > diff) {
    min = diff;
  }
  if(max < diff) {
    max = diff;
  }
  avg[times - 1] = diff;
  printf("\r%.1f%%", (float) times / 10);
  fflush(stdout);
  if(times++ == 1000) {
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
    time_manager_stop(data);
    canQuit = 1;
    pthread_exit(NULL);
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
  (void) pthread_mutex_lock(&mutexo);
  struct time_manager manager;
  start:
  printf("\rRound %d/3", counter + 1);
  fflush(stdout);
  int err = time_manager(&manager, 1, 1);
  if(err != time_success) {
    TEST_FAIL;
  }
  err = time_manager_start(&manager);
  if(err != time_success) {
    TEST_FAIL;
  }
  atomic_store(&last, 0);
  err = time_manager_add_timeout(&manager, time_get_ms(1500), cbfunc1, &manager, NULL);
  if(err != time_success) {
    TEST_FAIL;
  }
  err = time_manager_add_timeout(&manager, time_get_ms(500), cbfunc3, &manager, NULL);
  if(err != time_success) {
    TEST_FAIL;
  }
  err = time_manager_add_timeout(&manager, time_get_sec(1), cbfunc2, &manager, NULL);
  if(err != time_success) {
    TEST_FAIL;
  }
  (void) pthread_mutex_lock(&mutexo);
  time_manager_free(&manager);
  if(counter++ == 2) {
    puts("");
    TEST_PASS;
    goto benchmark;
  }
  goto start;
  benchmark:
  printf_debug("2. Benchmark", 1);
  puts("A \"frame loop\" will be created to see various statistics. An interval will be fired at 60FPS rate and the benchmark will end when 1000 samples have been collected.");
  err = time_manager(&manager, 1, 1);
  if(err != time_success) {
    TEST_FAIL;
  }
  err = time_manager_start(&manager);
  if(err != time_success) {
    TEST_FAIL;
  }
  begin = time_get_ns(16666666);
  last_time = begin - 16666666;
  times = 1;
  err = time_manager_add_interval(&manager, begin, 16666666, cbbenchfunc, &manager, NULL, time_instant);
  if(err != time_success) {
    TEST_FAIL;
  }
  while(canQuit == 0) {
    usleep(100000);
  }
  time_manager_free(&manager);
  return 0;
}