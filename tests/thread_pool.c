#include "tests.h"

#include <time.h>
#include <stdint.h>
#include <stdlib.h>
#include <shnet/threads.h>

const uint32_t amount = 1000000;

_Atomic uint32_t count;

void work(void* nil) {
  atomic_fetch_add(&count, 100);
}

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

void pool_work(void* nil) {
  for(unsigned long i = 0; i < 100; ++i) {
    atomic_fetch_add(&count, 100);
  }
  if(atomic_load(&count) == amount) {
    (void) pthread_mutex_unlock(&mutex);
  }
}

int main() {
  _debug("Testing thread_pool:", 1);
  
  (void) pthread_mutex_lock(&mutex);
  
  _debug("1. Control, x1 speed", 1);
  atomic_store(&count, 0);
  for(uint32_t i = 0; i < 10000; ++i) {
    work(NULL);
  }
  if(atomic_load(&count) != amount) {
    TEST_FAIL;
  }
  TEST_PASS;
  
  _debug("2. Experiment, x??? speed", 1);
  atomic_store(&count, 0);
  struct thread_pool pool = {0};
  if(thread_pool(&pool) == -1) {
    TEST_FAIL;
  }
  if(thread_pool_resize_raw(&pool, 100) == -1) {
    TEST_FAIL;
  }
  for(uint32_t i = 0; i < 100; ++i) {
    thread_pool_add_raw(&pool, pool_work, NULL);
  }
  struct threads threads = {0};
  if(threads_add(&threads, thread_pool_thread, &pool, 8) == -1) {
    TEST_FAIL;
  }
  (void) pthread_mutex_lock(&mutex);
  threads_shutdown(&threads);
  threads_free(&threads);
  TEST_PASS;
  
  _debug("3. Working manually", 1);
  for(uint32_t i = 0; i < 49; ++i) {
    thread_pool_add_raw(&pool, pool_work, NULL);
  }
  thread_pool_clear_raw(&pool);
  if(pool.used != 0) {
    TEST_FAIL;
  }
  for(uint32_t i = 0; i < 49; ++i) {
    thread_pool_add_raw(&pool, pool_work, NULL);
  }
  atomic_store(&count, 0);
  for(uint32_t i = 0; i < 49; ++i) {
    thread_pool_work_raw(&pool);
  }
  if(atomic_load(&count) != 490000) {
    TEST_FAIL;
  }
  if(pool.used != 0) {
    TEST_FAIL;
  }
  TEST_PASS;
  
  
  
  _debug("Testing thread_pool succeeded", 1);
  (void) pthread_mutex_unlock(&mutex);
  (void) pthread_mutex_destroy(&mutex);
  thread_pool_free(&pool);
  debug_free();
  return 0;
}