#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdatomic.h>

#include <shnet/error.h>
#include <shnet/threads.h>

void pthread_cancel_on() {
  (void) pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
}

void pthread_cancel_off() {
  (void) pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);
}

int pthread_start_explicit(pthread_t* id, const pthread_attr_t* const attr, void* (*func)(void*), void* const data) {
  int err;
  pthread_t tid;
  if(id == NULL) {
    id = &tid;
  }
  safe_execute(err = pthread_create(id, attr, func, data), err != 0, err);
  if(err != 0) {
    errno = err;
    return -1;
  }
  return 0;
}

int pthread_start(pthread_t* const id, void* (*func)(void*), void* const data) {
  return pthread_start_explicit(id, NULL, func, data);
}

void pthread_cancel_sync(const pthread_t id) {
  if(pthread_equal(id, pthread_self())) {
    (void) pthread_detach(id);
    pthread_exit(NULL);
  } else {
    (void) pthread_cancel(id);
    (void) pthread_join(id, NULL);
  }
}

void pthread_cancel_async(const pthread_t id) {
  (void) pthread_detach(id);
  (void) pthread_cancel(id);
}



#define data ((struct _pthreads_data*) pthreads_thread_data)

static void* pthreads_thread(void* pthreads_thread_data) {
  /*
   * Notes for future me:
   * NO, pthread_barrier_t may not be used here, because
   * it cannot be interrupted by a cancellation request,
   * which alone defeats the whole point of this system.
   * Jesus...     This took too many tries to get right.
   */
  (void) sem_wait(&data->sem);
  void* (*func)(void*) = data->func;
  void* const arg = data->arg;
  if(atomic_fetch_sub_explicit(&data->count, 1, memory_order_relaxed) == 1) {
    (void) pthread_mutex_unlock(&data->mutex);
  }
  return func(arg);
}

#undef data

int pthreads_resize(pthreads_t* const threads, const uint32_t new_size) {
  void* ptr;
  safe_execute(ptr = realloc(threads->ids, sizeof(*threads->ids) * new_size), ptr == NULL, ENOMEM);
  if(ptr == NULL) {
    return -1;
  }
  threads->ids = ptr;
  threads->size = new_size;
  return 0;
}

int pthreads_start_explicit(pthreads_t* const threads, const pthread_attr_t* const attr, void* (*func)(void*), void* const arg, const uint32_t amount) {
  const uint32_t total = threads->used + amount;
  if(total > threads->size && pthreads_resize(threads, total) == -1) {
    return -1;
  }
  struct _pthreads_data* data;
  safe_execute(data = malloc(sizeof(*data)), data == NULL, ENOMEM);
  if(data == NULL) {
    return -1;
  }
  int err;
  safe_execute(err = sem_init(&data->sem, 0, 0), err == -1, errno);
  if(err == -1) {
    free(data);
    return -1;
  }
  safe_execute(err = pthread_mutex_init(&data->mutex, NULL), err != 0, err);
  if(err != 0) {
    (void) sem_destroy(&data->sem);
    free(data);
    return -1;
  }
  data->arg = arg;
  data->func = func;
  atomic_store_explicit(&data->count, amount, memory_order_relaxed);
  for(uint32_t i = 0; i < amount; ++i, ++threads->used) {
    if(pthread_start_explicit(threads->ids + threads->used, attr, pthreads_thread, data)) {
      pthreads_cancel_sync(threads, i);
      (void) pthread_mutex_destroy(&data->mutex);
      (void) sem_destroy(&data->sem);
      free(data);
      return -1;
    }
  }
  (void) pthread_mutex_lock(&data->mutex);
  for(uint32_t i = 0; i < amount; ++i) {
    (void) sem_post(&data->sem);
  }
  /*
   * Another note for future me:
   * Confirmation is required. Otherwise, pthreads_cancel_*()
   * will cancel our threads before they get the data freed.
   */
  (void) pthread_mutex_lock(&data->mutex);
  (void) pthread_mutex_unlock(&data->mutex);
  (void) pthread_mutex_destroy(&data->mutex);
  (void) sem_destroy(&data->sem);
  free(data);
  return 0;
}

int pthreads_start(pthreads_t* const threads, void* (*func)(void*), void* const arg, const uint32_t amount) {
  return pthreads_start_explicit(threads, NULL, func, arg, amount);
}

void pthreads_cancel(pthreads_t* const threads, const uint32_t amount) {
  if(!amount) {
    return;
  }
  const uint32_t total = threads->used - amount;
  const pthread_t self = pthread_self();
  int close_ourselves = 0;
  for(uint32_t i = total; i < threads->used; ++i) {
    if(!pthread_equal(threads->ids[i], self)) {
      (void) pthread_cancel(threads->ids[i]);
    } else {
      close_ourselves = 1;
    }
  }
  threads->used = total;
  if(close_ourselves == 1) {
    (void) pthread_cancel(self);
  }
}

void pthreads_cancel_sync(pthreads_t* const threads, const uint32_t amount) {
  if(!amount) {
    return;
  }
  const uint32_t total = threads->used - amount;
  const pthread_t self = pthread_self();
  int close_ourselves = 0;
  for(uint32_t i = total; i < threads->used; ++i) {
    if(!pthread_equal(threads->ids[i], self)) {
      (void) pthread_cancel(threads->ids[i]);
    } else {
      close_ourselves = 1;
    }
  }
  for(uint32_t i = total; i < threads->used; ++i) {
    if(!pthread_equal(threads->ids[i], self)) {
      (void) pthread_join(threads->ids[i], NULL);
    }
  }
  threads->used = total;
  if(close_ourselves == 1) {
    (void) pthread_detach(self);
    pthread_exit(NULL);
  }
}

void pthreads_cancel_async(pthreads_t* const threads, const uint32_t amount) {
  if(!amount) {
    return;
  }
  const uint32_t total = threads->used - amount;
  const pthread_t self = pthread_self();
  int close_ourselves = 0;
  for(uint32_t i = total; i < threads->used; ++i) {
    if(!pthread_equal(threads->ids[i], self)) {
      (void) pthread_detach(threads->ids[i]);
      (void) pthread_cancel(threads->ids[i]);
    } else {
      close_ourselves = 1;
    }
  }
  threads->used = total;
  if(close_ourselves == 1) {
    (void) pthread_detach(self);
    (void) pthread_cancel(self);
  }
}

void pthreads_shutdown(pthreads_t* const threads) {
  pthreads_cancel(threads, threads->used);
}

void pthreads_shutdown_sync(pthreads_t* const threads) {
  pthreads_cancel_sync(threads, threads->used);
}

void pthreads_shutdown_async(pthreads_t* const threads) {
  pthreads_cancel_async(threads, threads->used);
}

void pthreads_free(pthreads_t* const threads) {
  free(threads->ids);
  threads->ids = NULL;
  threads->used = 0;
  threads->size = 0;
}



#define pool ((struct thread_pool*) thread_pool_thread_data)

void* thread_pool_thread(void* thread_pool_thread_data) {
  while(1) {
    thread_pool_work(pool);
  }
  assert(0);
}

#undef pool

int thread_pool(struct thread_pool* const pool) {
  int err;
  safe_execute(err = pthread_mutex_init(&pool->mutex, NULL), err != 0, err);
  if(err != 0) {
    errno = err;
    return -1;
  }
  safe_execute(err = sem_init(&pool->sem, 0, 0), err == -1, errno);
  if(err == -1) {
    (void) pthread_mutex_destroy(&pool->mutex);
  }
  return err;
}

void thread_pool_lock(struct thread_pool* const pool) {
  (void) pthread_mutex_lock(&pool->mutex);
}

void thread_pool_unlock(struct thread_pool* const pool) {
  (void) pthread_mutex_unlock(&pool->mutex);
}

int thread_pool_resize_raw(struct thread_pool* const pool, const uint32_t new_size) {
  void* ptr;
  safe_execute(ptr = realloc(pool->queue, sizeof(*pool->queue) * new_size), ptr == NULL, ENOMEM);
  if(ptr == NULL) {
    return -1;
  }
  pool->queue = ptr;
  pool->size = new_size;
  return 0;
}

int thread_pool_resize(struct thread_pool* const pool, const uint32_t new_size) {
  thread_pool_lock(pool);
  const int ret = thread_pool_resize_raw(pool, new_size);
  thread_pool_unlock(pool);
  return ret;
}

int thread_pool_add_raw(struct thread_pool* const pool, void (*func)(void*), void* const data) {
  if(pool->used >= pool->size && thread_pool_resize(pool, pool->used + 1) == -1) {
    return -1;
  }
  pool->queue[pool->used++] = (struct thread_pool_job) { .func = func, .data = data };
  (void) sem_post(&pool->sem);
  return 0;
}

int thread_pool_add(struct thread_pool* const pool, void (*func)(void*), void* const data) {
  thread_pool_lock(pool);
  if(pool->used >= pool->size && thread_pool_resize(pool, pool->used + 1) == -1) {
    thread_pool_unlock(pool);
    return -1;
  }
  pool->queue[pool->used++] = (struct thread_pool_job) { .func = func, .data = data };
  thread_pool_unlock(pool);
  (void) sem_post(&pool->sem);
  return 0;
}

void thread_pool_try_work_raw(struct thread_pool* const pool) {
  if(pool->used != 0) {
    struct thread_pool_job data;
    data.func = pool->queue->func;
    data.data = pool->queue->data;
    --pool->used;
    (void) memmove(pool->queue, pool->queue + 1, sizeof(*pool->queue) * pool->used);
    data.func(data.data);
  }
}

void thread_pool_try_work(struct thread_pool* const pool) {
  thread_pool_lock(pool);
  if(pool->used != 0) {
    struct thread_pool_job data;
    data.func = pool->queue->func;
    data.data = pool->queue->data;
    --pool->used;
    (void) memmove(pool->queue, pool->queue + 1, sizeof(*pool->queue) * pool->used);
    thread_pool_unlock(pool);
    data.func(data.data);
  } else {
    thread_pool_unlock(pool);
  }
}

void thread_pool_work_raw(struct thread_pool* const pool) {
  (void) sem_wait(&pool->sem);
  thread_pool_try_work_raw(pool);
}

void thread_pool_work(struct thread_pool* const pool) {
  (void) sem_wait(&pool->sem);
  thread_pool_try_work(pool);
}

void thread_pool_clear_raw(struct thread_pool* const pool) {
  pool->used = 0;
}

void thread_pool_clear(struct thread_pool* const pool) {
  thread_pool_lock(pool);
  thread_pool_clear_raw(pool);
  thread_pool_unlock(pool);
}

void thread_pool_free(struct thread_pool* const pool) {
  (void) sem_destroy(&pool->sem);
  (void) pthread_mutex_destroy(&pool->mutex);
  free(pool->queue);
  pool->queue = NULL;
  pool->used = 0;
  pool->size = 0;
}