#include <shnet/test.h>

#include <string.h>
#include <stdatomic.h>

#include <shnet/threads.h>

const uint32_t target = 1000;

_Atomic uint32_t count;

uint32_t get_count(void) {
  return atomic_load_explicit(&count, memory_order_relaxed);
}

void set_count(const uint32_t a) {
  atomic_store_explicit(&count, a, memory_order_relaxed);
}

void add_count(const uint32_t a) {
  atomic_fetch_add_explicit(&count, a, memory_order_relaxed);
}

void work(void* nil) {
  add_count(1);
}

void pool_work(void* nil) {
  for(unsigned long i = 0; i < 100; ++i) {
    add_count(1);
  }
  if(get_count() == target) {
    test_wake();
  }
}

test_register(void*, shnet_realloc, (void* const a, const size_t b), (a, b))
test_register(int, sem_init, (sem_t* a, int b, unsigned int c), (a, b, c))
test_register(int, pthread_mutex_init, (pthread_mutex_t* restrict a, const pthread_mutexattr_t* restrict b), (a, b))

int main() {
  test_begin("thread pool check");
  test_error_check(void*, shnet_realloc, ((void*) 0xbad, 0xbad));
  test_error_check(int, sem_init, ((void*) 0xbad, 0xbad, 0xbad));
  test_error_check(int, pthread_mutex_init, ((void*) 0x1bad, (void*) 0x2bad));
  
  test_error_set_retval(shnet_realloc, NULL);
  test_error_set_retval(pthread_mutex_init, ENOMEM);
  test_end();
  
  struct thread_pool pool = {0};
  struct thread_pool temp;
  set_count(0);
  
  test_begin("thread pool init err 1");
  test_error(pthread_mutex_init);
  assert(thread_pool(&pool));
  assert(pool.used == 0);
  assert(pool.size == 0);
  assert(pool.queue == NULL);
  test_end();
  
  test_begin("thread pool init err 2");
  test_error(sem_init);
  assert(thread_pool(&pool));
  assert(pool.used == 0);
  assert(pool.size == 0);
  assert(pool.queue == NULL);
  test_end();
  
  test_begin("thread pool init");
  assert(!thread_pool(&pool));
  assert(pool.used == 0);
  assert(pool.size == 0);
  assert(pool.queue == NULL);
  thread_pool_free(&pool);
  assert(!thread_pool(&pool));
  assert(pool.used == 0);
  assert(pool.size == 0);
  assert(pool.queue == NULL);
  test_end();
  
  test_begin("thread pool resize raw 1");
  assert(!thread_pool_resize_raw(&pool, 1));
  assert(pool.used == 0);
  assert(pool.size == 1);
  assert(pool.queue);
  test_end();
  
  test_begin("thread pool resize raw 2");
  thread_pool_lock(&pool);
  assert(!thread_pool_resize_raw(&pool, 1));
  thread_pool_unlock(&pool);
  assert(pool.used == 0);
  assert(pool.size == 1);
  assert(pool.queue);
  test_end();
  
  test_begin("thread pool resize raw err 1");
  test_error(shnet_realloc);
  temp = pool;
  assert(thread_pool_resize_raw(&pool, 0xbad));
  assert(!memcmp(&temp, &pool, sizeof(pool)));
  test_end();
  
  test_begin("thread pool resize raw err 2");
  test_error(shnet_realloc);
  thread_pool_lock(&pool);
  temp = pool;
  assert(thread_pool_resize_raw(&pool, 0xbad));
  assert(!memcmp(&temp, &pool, sizeof(pool)));
  thread_pool_unlock(&pool);
  test_end();
  
  test_begin("thread pool resize raw 0 1");
  assert(!thread_pool_resize_raw(&pool, 0));
  assert(pool.used == 0);
  assert(pool.size == 0);
  assert(pool.queue == NULL);
  test_end();
  
  test_begin("thread pool resize raw 0 2");
  thread_pool_lock(&pool);
  assert(!thread_pool_resize_raw(&pool, 0));
  thread_pool_unlock(&pool);
  assert(pool.used == 0);
  assert(pool.size == 0);
  assert(pool.queue == NULL);
  test_end();
  
  test_begin("thread pool resize");
  assert(!thread_pool_resize(&pool, 1));
  assert(pool.used == 0);
  assert(pool.size == 1);
  assert(pool.queue);
  test_end();
  
  test_begin("thread pool resize err");
  test_error(shnet_realloc);
  temp = pool;
  assert(thread_pool_resize(&pool, 0xbad));
  assert(!memcmp(&temp, &pool, sizeof(pool)));
  test_end();
  
  test_begin("thread pool resize 0");
  assert(!thread_pool_resize(&pool, 0));
  assert(pool.used == 0);
  assert(pool.size == 0);
  assert(pool.queue == NULL);
  test_end();
  
  test_begin("thread pool add raw err 1");
  test_error(shnet_realloc);
  temp = pool;
  assert(thread_pool_add_raw(&pool, (void (*)(void*)) 0xbad, (void*) 0xbad));
  assert(!memcmp(&temp, &pool, sizeof(pool)));
  test_end();
  
  test_begin("thread pool add raw err 2");
  test_error(shnet_realloc);
  thread_pool_lock(&pool);
  temp = pool;
  assert(thread_pool_add_raw(&pool, (void (*)(void*)) 0xbad, (void*) 0xbad));
  assert(!memcmp(&temp, &pool, sizeof(pool)));
  thread_pool_unlock(&pool);
  test_end();
  
  test_begin("thread pool add err");
  test_error(shnet_realloc);
  temp = pool;
  assert(thread_pool_add(&pool, (void (*)(void*)) 0xbad, (void*) 0xbad));
  assert(!memcmp(&temp, &pool, sizeof(pool)));
  test_end();
  
  test_begin("thread pool add raw 1");
  assert(!thread_pool_add_raw(&pool, work, NULL));
  assert(pool.used == 1);
  thread_pool_clear(&pool);
  test_end();
  
  test_begin("thread pool add raw 2");
  thread_pool_lock(&pool);
  assert(!thread_pool_add_raw(&pool, work, NULL));
  thread_pool_unlock(&pool);
  assert(pool.used == 1);
  thread_pool_clear_raw(&pool);
  test_end();
  
  test_begin("thread pool add");
  assert(!thread_pool_add(&pool, work, NULL));
  assert(pool.used == 1);
  test_end();
  
  test_begin("thread pool try work raw 1");
  assert(get_count() == 0);
  thread_pool_try_work_raw(&pool);
  assert(get_count() == 1);
  assert(pool.used == 0);
  assert(!thread_pool_add_raw(&pool, work, NULL));
  set_count(0);
  test_end();
  
  test_begin("thread pool try work raw 2");
  assert(get_count() == 0);
  thread_pool_lock(&pool);
  thread_pool_try_work_raw(&pool);
  thread_pool_unlock(&pool);
  assert(get_count() == 1);
  assert(pool.used == 0);
  assert(!thread_pool_add_raw(&pool, work, NULL));
  set_count(0);
  test_end();
  
  test_begin("thread pool try work");
  assert(get_count() == 0);
  thread_pool_try_work(&pool);
  assert(get_count() == 1);
  assert(pool.used == 0);
  set_count(0);
  test_end();
  
  test_begin("thread pool try work raw empty 1");
  assert(get_count() == 0);
  temp = pool;
  thread_pool_try_work_raw(&pool);
  assert(!memcmp(&temp, &pool, sizeof(pool)));
  assert(get_count() == 0);
  test_end();
  
  test_begin("thread pool try work raw empty 2");
  assert(get_count() == 0);
  thread_pool_lock(&pool);
  temp = pool;
  thread_pool_try_work_raw(&pool);
  assert(!memcmp(&temp, &pool, sizeof(pool)));
  thread_pool_unlock(&pool);
  assert(get_count() == 0);
  test_end();
  
  test_begin("thread pool try work empty");
  assert(get_count() == 0);
  temp = pool;
  thread_pool_try_work(&pool);
  assert(!memcmp(&temp, &pool, sizeof(pool)));
  assert(get_count() == 0);
  test_end();
  
  test_begin("thread pool drain");
  while(sem_trywait(&pool.sem) == 0);
  test_end();
  
  test_begin("thread pool work raw 1");
  assert(get_count() == 0);
  thread_pool_lock(&pool);
  for(int i = 0; i < 10; ++i) {
    assert(!thread_pool_add_raw(&pool, work, NULL));
  }
  thread_pool_unlock(&pool);
  for(int i = 0; i < 9; ++i) {
    thread_pool_work_raw(&pool);
  }
  assert(get_count() == 9);
  assert(pool.used == 1);
  set_count(0);
  test_end();
  
  test_begin("thread pool work raw 2");
  assert(get_count() == 0);
  for(int i = 0; i < 6; ++i) {
    assert(!thread_pool_add(&pool, work, NULL));
  }
  thread_pool_lock(&pool);
  for(int i = 0; i < 7; ++i) {
    thread_pool_work_raw(&pool);
  }
  thread_pool_unlock(&pool);
  assert(get_count() == 7);
  assert(pool.used == 0);
  set_count(0);
  test_end();
  
  test_begin("thread pool work");
  assert(get_count() == 0);
  for(int i = 0; i < 13; ++i) {
    assert(!thread_pool_add_raw(&pool, work, NULL));
  }
  thread_pool_unlock(&pool);
  for(int i = 0; i < 13; ++i) {
    thread_pool_work(&pool);
  }
  assert(get_count() == 13);
  assert(pool.used == 0);
  set_count(0);
  test_end();
  
  test_begin("thread pool thread");
  pthreads_t threads = {0};
  assert(!pthreads_start(&threads, thread_pool_thread, &pool, 4));
  for(int i = 0; i < 10; ++i) {
    assert(!thread_pool_add(&pool, pool_work, NULL));
  }
  test_wait();
  assert(get_count() == target);
  pthreads_shutdown_sync(&threads);
  pthreads_free(&threads);
  test_end();
  
  test_begin("thread pool free");
  thread_pool_free(&pool);
  assert(pool.used == 0);
  assert(pool.size == 0);
  assert(pool.queue == NULL);
  test_end();
  
  return 0;
}
