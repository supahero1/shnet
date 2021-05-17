#include "tests.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdatomic.h>
#include <shnet/threads.h>

#define thread_count 100UL

_Atomic unsigned long first = 0;

void onstart(struct threads* a) {
  int err = threads_shutdown(a);
  if(err != threads_success) {
    TEST_FAIL;
  }
}

void onstop(struct threads* a) {
  if(atomic_fetch_add(&first, 1) == 2) {
    return;
  }
  int err = threads_add(a, thread_count, threads_detached);
  if(err != threads_success) {
    if(err == threads_out_of_memory) {
      TEST_FAIL;
    }
    TEST_FAIL;
  }
}

void cb(void* data) {
  sleep(9999);
}

int main() {
  printf_debug("Testing threads.c:", 1);
  printf_debug("This usually takes exactly a second, but it MIGHT take longer. Be patient, or use an interrupt (^C) to break out of the test.", 1);
  struct threads threads;
  memset(&threads, 0, sizeof(struct threads));
  threads.on_start = onstart;
  threads.on_stop = onstop;
  threads.startup = cb;
  threads.data = &threads;
  int err = threads_add(&threads, thread_count, threads_detached);
  if(err != threads_success) {
    TEST_FAIL;
  }
  while(1) {
    sleep(1);
    if(atomic_load(&first) == 3) {
      TEST_PASS;
      return 0;
    }
  }
  return 0;
}