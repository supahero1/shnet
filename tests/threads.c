#include "tests.h"

#include <time.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <shnet/threads.h>

#define thread_count 50UL
#define repeat 2000

void cb(void* data) {
  usleep(30000000);
  TEST_FAIL;
}

int main() {
  _debug("Testing threads:", 1);
  {
    struct timespec tp;
    (void) clock_gettime(CLOCK_REALTIME, &tp);
    srand(tp.tv_nsec + tp.tv_sec * 1000000000);
  }
  
  _debug("1. Synchronous stress test", 1);
  
  _debug("1.1.", 1);
  struct threads threads = {0};
  if(threads_resize(&threads, thread_count << 1) == -1) {
    TEST_FAIL;
  }
  (void) threads_add(&threads, cb, &threads, thread_count);
  for(uint32_t i = 0; i < repeat; ++i) {
    printf("\r%.1f%%", (float)(i + 1) / repeat * 100);
    fflush(stdout);
    threads_remove(&threads, 20 + (rand() % (threads.used - 19)));
    threads_add(&threads, cb, &threads, 20 + (rand() % (thread_count - threads.used)));
  }
  threads_shutdown(&threads);
  printf("\r");
  TEST_PASS;
  
  _debug("1.2.", 1);
  for(uint32_t i = 0; i < repeat; ++i) {
    printf("\r%.1f%%", (float)(i + 1) / repeat * 100);
    fflush(stdout);
    threads_add(&threads, cb, &threads, 20 + (rand() % (thread_count - threads.used)));
    threads_remove(&threads, 20 + (rand() % (threads.used - 19)));
  }
  threads_shutdown(&threads);
  printf("\r");
  TEST_PASS;
  
  _debug("2. Asynchronous stress test", 1);
  
  _debug("2.1.", 1);
  for(uint32_t i = 0; i < repeat; ++i) {
    printf("\r%.1f%%", (float)(i + 1) / repeat * 100);
    fflush(stdout);
    threads_add(&threads, cb, &threads, 20 + (rand() % (thread_count - threads.used)));
    threads_remove_async(&threads, 20 + (rand() % (threads.used - 19)));
  }
  threads_shutdown_async(&threads);
  printf("\r");
  TEST_PASS;
  
  _debug("2.2.", 1);
  for(uint32_t i = 0; i < repeat; ++i) {
    printf("\r%.1f%%", (float)(i + 1) / repeat * 100);
    fflush(stdout);
    threads_add(&threads, cb, &threads, 20 + (rand() % (thread_count - threads.used)));
    threads_remove_async(&threads, 20 + (rand() % (threads.used - 19)));
  }
  threads_shutdown(&threads);
  printf("\r");
  TEST_PASS;
  
  _debug("Testing threads succeeded", 1);
  threads_free(&threads);
  debug_free();
  return 0;
}