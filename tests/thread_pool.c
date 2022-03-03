#include <shnet/tests.h>

#include <stdatomic.h>

#include <shnet/threads.h>

const uint32_t amount = 1000000;

_Atomic uint32_t count;

void work(void* nil) {
  atomic_fetch_add_explicit(&count, 100, memory_order_relaxed);
}

void pool_work(void* nil) {
  for(unsigned long i = 0; i < 100; ++i) {
    atomic_fetch_add_explicit(&count, 100, memory_order_relaxed);
  }
  if(atomic_load_explicit(&count, memory_order_relaxed) == amount) {
    test_wake();
  }
}

int main() {
  begin_test("thread pool control group");
  atomic_store(&count, 0);
  for(uint32_t i = 0; i < 10000; ++i) {
    work(NULL);
  }
  assert(atomic_load_explicit(&count, memory_order_relaxed) == amount);
  end_test();
  
  begin_test("thread pool experiment group");
  atomic_store(&count, 0);
  struct thread_pool pool = {0};
  assert(thread_pool(&pool) != -1);
  assert(thread_pool_resize_raw(&pool, 100) != -1);
  for(uint32_t i = 0; i < 100; ++i) {
    thread_pool_add_raw(&pool, pool_work, NULL);
  }
  pthreads_t threads = {0};
  assert(pthreads_start(&threads, thread_pool_thread, &pool, 8) != -1);
  test_wait();
  pthreads_shutdown_sync(&threads);
  pthreads_free(&threads);
  end_test();
  
  begin_test("thread pool manual work");
  for(uint32_t i = 0; i < 49; ++i) {
    thread_pool_add_raw(&pool, pool_work, NULL);
  }
  thread_pool_clear_raw(&pool);
  assert(pool.used == 0);
  for(uint32_t i = 0; i < 49; ++i) {
    thread_pool_add_raw(&pool, pool_work, NULL);
  }
  atomic_store(&count, 0);
  for(uint32_t i = 0; i < 49; ++i) {
    thread_pool_work_raw(&pool);
  }
  assert(atomic_load_explicit(&count, memory_order_relaxed) == 490000);
  atomic_store_explicit(&count, 0, memory_order_relaxed);
  assert(pool.used == 0);
  end_test();
  
  begin_test("thread pool");
  thread_pool_lock(&pool);
  thread_pool_try_work_raw(&pool);
  thread_pool_unlock(&pool);
  assert(atomic_load_explicit(&count, memory_order_relaxed) == 0);
  end_test();
  
  thread_pool_free(&pool);
  
  return 0;
}