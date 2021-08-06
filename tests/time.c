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
  if(time_manager_add_timeout(data, time_get_ms(500), cbfunc4, data, NULL) != 0) {
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
  if(atomic_fetch_add(&last, 1) == 0) {
    for(int i = 0; i < 11; ++i) {
      if(time_manager_add_timeout(data, time_get_ms(500), cbfunc4, data, NULL) != 0) {
        TEST_FAIL;
      }
    }
    if(time_manager_add_timeout(data, time_get_sec(1), cbfunc5, data, NULL) != 0) {
      TEST_FAIL;
    }
    struct time_reference timeout = {0};
    struct time_reference timeout2 = {0};
    struct time_reference interval = {0};
    if(time_manager_add_timeout(data, time_get_ms(750), cbfunc6, data, &timeout) != 0) {
      TEST_FAIL;
    }
    if(time_manager_add_interval(data, time_get_ms(100), time_ms_to_ns(100), cbfunc6, data, &interval) != 0) {
      TEST_FAIL;
    }
    if(time_manager_add_timeout(data, time_get_ms(5), cbfunc6, data, &timeout2) != 0) {
      TEST_FAIL;
    }
    time_manager_cancel_timeout(data, &timeout);
    time_manager_cancel_timeout(data, &timeout2);
    time_manager_cancel_interval(data, &interval);
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
uint64_t min = UINT64_MAX;
uint64_t max = 0;
#define AMOUNT 500
uint64_t avg[AMOUNT];

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
  printf("\r%.1f%%", (float) times / 10 * (1000 / AMOUNT));
  fflush(stdout);
  if(times++ == AMOUNT) {
    float average = 0;
    for(int i = 0; i < AMOUNT; ++i) {
      average += (float) avg[i] / 1000000;
    }
    average /= AMOUNT;
    float sd = 0;
    for(int i = 0; i < AMOUNT; ++i) {
      sd += ((float) avg[i] / 1000000 - average) * ((float) avg[i] / 1000000 - average);
    }
    sd /= AMOUNT;
    sd = sqrt(sd);
    _debug("\rAverage: %.2f ms\nMin: %.2f ms\nMax: %.2f ms\nStandard deviation: %.2f", 1, average, (float) min / 1000000, (float) max / 1000000, sd);
    TEST_PASS;
    time_manager_stop(data);
    pthread_mutex_unlock(&mutexo);
    pthread_exit(NULL);
  }
}

int counter = 0;

int main() {
  _debug("Testing time:", 1);
  _debug("1. Stress test", 1);
  puts("If a round takes longer than 5 seconds, consider the test failed and feel free to break out using an interrupt (^C).");
  srand(time_get_time());
  (void) pthread_mutex_lock(&mutexo);
  struct time_manager manager;
  start:
  printf("\rRound %d/3", counter + 1);
  fflush(stdout);
  memset(&manager, 0, sizeof(manager));
  if(time_manager(&manager) != 0) {
    TEST_FAIL;
  }
  if(time_manager_start(&manager) != 0) {
    TEST_FAIL;
  }
  atomic_store(&last, 0);
  if(time_manager_add_timeout(&manager, time_get_ms(1500), cbfunc1, &manager, NULL) != 0) {
    TEST_FAIL;
  }
  if(time_manager_add_timeout(&manager, time_get_ms(500), cbfunc3, &manager, NULL) != 0) {
    TEST_FAIL;
  }
  if(time_manager_add_timeout(&manager, time_get_sec(1), cbfunc2, &manager, NULL) != 0) {
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
  _debug("2. Benchmark", 1);
  printf("An interval will be fired at 60FPS rate to see various statistics. The benchmark will end once %d samples are collected.\n", AMOUNT);
  memset(&manager, 0, sizeof(manager));
  if(time_manager(&manager) != 0) {
    TEST_FAIL;
  }
  if(time_manager_start(&manager) != 0) {
    TEST_FAIL;
  }
  begin = time_get_ns(16666666);
  last_time = begin - 16666666;
  times = 1;
  if(time_manager_add_interval(&manager, begin, 16666666, cbbenchfunc, &manager, NULL) != 0) {
    TEST_FAIL;
  }
  pthread_mutex_lock(&mutexo);
  time_manager_free(&manager);
  return 0;
}