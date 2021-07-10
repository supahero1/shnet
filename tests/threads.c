#include "tests.h"

#include <time.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <shnet/threads.h>

#define thread_count 100UL
#define repeat 1000

void cb(void* data) {
  (void) pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL);
  usleep(5000000);
  TEST_FAIL;
}

int main() {
  printf_debug("Testing threads:", 1);
  {
    struct timespec tp;
    (void) clock_gettime(CLOCK_REALTIME, &tp);
    srand(tp.tv_nsec + tp.tv_sec * 1000000000);
  }
  struct threads thrds;
  memset(&thrds, 0, sizeof(thrds));
  int err = threads(&thrds);
  if(err != 0) {
    TEST_FAIL;
  }
  thrds.func = cb;
  thrds.data = &thrds;
  err = threads_add(&thrds, thread_count);
  if(err != 0) {
    TEST_FAIL;
  }
  unsigned long how_much;
  for(unsigned long i = 0; i < repeat; ++i) {
    printf("\r%.1f%%", (float)(i + 1) / repeat / 2 * 100);
    fflush(stdout);
    how_much = 1 + (rand() % thrds.used);
    threads_remove(&thrds, how_much);
    how_much = 1 + (rand() % (thread_count - thrds.used));
    err = threads_add(&thrds, how_much);
    if(err != 0) {
      printf("\r");
      if(errno == ENOMEM) {
        TEST_FAIL;
      }
      TEST_FAIL;
    }
  }
  if(thrds.used != 0) {
    threads_shutdown(&thrds);
  }
  for(unsigned long i = 0; i < repeat; ++i) {
    printf("\r%.1f%%", (float)((i + repeat) + 1) / repeat / 2 * 100);
    fflush(stdout);
    how_much = 1 + (rand() % (thread_count - thrds.used));
    err = threads_add(&thrds, how_much);
    if(err != 0) {
      printf("\r");
      if(errno == ENOMEM) {
        TEST_FAIL;
      }
      TEST_FAIL;
    }
    how_much = 1 + (rand() % thrds.used);
    threads_remove(&thrds, how_much);
  }
  if(thrds.used != 0) {
    threads_shutdown(&thrds);
  }
  threads_free(&thrds);
  printf("\r");
  TEST_PASS;
  return 0;
}