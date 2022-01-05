#include "error.h"
#include "threads.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>

void thread_cancellation_disable() {
  (void) pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);
}

void thread_cancellation_enable() {
  (void) pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
}

void thread_cancellation_async() {
  (void) pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL);
}

void thread_cancellation_deferred() {
  (void) pthread_setcanceltype(PTHREAD_CANCEL_DEFERRED, NULL);
}

#define thrd ((struct thread_data*) thread_thread_data)

static void* thread_thread(void* thread_thread_data) {
  void (*func)(void*) = thrd->func;
  void* const data = thrd->data;
  free(thrd);
  func(data);
  return NULL;
}

#undef thrd

int thread_start(struct thread* const thread, void (*func)(void*), void* const data_) {
  struct thread_data* data;
  safe_execute(data = malloc(sizeof(struct thread_data)), data == NULL, ENOMEM);
  if(data == NULL) {
    return -1;
  }
  data->func = func;
  data->data = data_;
  int err;
  safe_execute(err = pthread_create(&thread->thread, NULL, thread_thread, data), err != 0, err);
  if(err != 0) {
    free(data);
    errno = err;
    return -1;
  }
  return 0;
}

void thread_stop(struct thread* const thread) {
  if(pthread_equal(thread->thread, pthread_self())) {
    (void) pthread_detach(thread->thread);
    pthread_cancel(thread->thread);
  } else {
    pthread_cancel(thread->thread);
    (void) pthread_join(thread->thread, NULL);
  }
}

void thread_stop_async(struct thread* const thread) {
  (void) pthread_detach(thread->thread);
  pthread_cancel(thread->thread);
}



#define thrd ((struct threads_data*) threads_thread_data)

static void* threads_thread(void* threads_thread_data) {
  (void) pthread_barrier_wait(&thrd->barrier);
  void (*func)(void*) = thrd->func;
  void* const data = thrd->data;
  if(atomic_fetch_sub(&thrd->count, 1) == 1) {
    (void) pthread_mutex_unlock(thrd->mutex);
  }
  func(data);
  return NULL;
}

#undef thrd

int threads_resize(struct threads* const threads, const uint32_t new_size) {
  pthread_t* ptr;
  safe_execute(ptr = realloc(threads->threads, sizeof(pthread_t) * new_size), ptr == NULL, ENOMEM);
  if(ptr == NULL) {
    return -1;
  }
  threads->threads = ptr;
  threads->size = new_size;
  return 0;
}

int threads_add(struct threads* const threads, void (*func)(void*), void* const data_, const uint32_t amount) {
  const uint32_t total = threads->used + amount;
  if(total > threads->size && threads_resize(threads, total) == -1) {
    return -1;
  }
  struct threads_data* data;
  safe_execute(data = malloc(sizeof(struct threads_data)), data == NULL, ENOMEM);
  if(data == NULL) {
    return -1;
  }
  int err;
  safe_execute(err = pthread_barrier_init(&data->barrier, NULL, amount), err != 0, err);
  if(err != 0) {
    free(data);
    errno = err;
    return -1;
  }
  data->data = data_;
  data->func = func;
  pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
  (void) pthread_mutex_lock(&mutex);
  data->mutex = &mutex;
  atomic_store_explicit(&data->count, amount, memory_order_relaxed);
  for(uint32_t i = threads->used; i < total; ++i) {
    safe_execute(err = pthread_create(threads->threads + i, NULL, threads_thread, data), err != 0, err);
    if(err != 0) {
      if(i > threads->used) {
        const uint32_t old = threads->used;
        threads->used = i;
        (void) threads_remove_async(threads, i - threads->used);
        threads->used = old;
      }
      (void) pthread_barrier_destroy(&data->barrier);
      (void) pthread_mutex_destroy(&mutex);
      free(data);
      errno = err;
      return -1;
    }
  }
  threads->used = total;
  (void) pthread_mutex_lock(&mutex);
  (void) pthread_mutex_unlock(&mutex);
  (void) pthread_mutex_destroy(&mutex);
  (void) pthread_barrier_destroy(&data->barrier);
  free(data);
  return 0;
}

void threads_remove(struct threads* const threads, const uint32_t amount) {
  const uint32_t total = threads->used - amount;
  const pthread_t self = pthread_self();
  int close_ourselves = 0;
  for(uint32_t i = total; i < threads->used; ++i) {
    if(pthread_equal(threads->threads[i], self) == 0) {
      (void) pthread_cancel(threads->threads[i]);
    } else {
      close_ourselves = 1;
    }
  }
  for(uint32_t i = total; i < threads->used; ++i) {
    if(pthread_equal(threads->threads[i], self) == 0) {
      (void) pthread_join(threads->threads[i], NULL);
    }
  }
  threads->used = total;
  if(close_ourselves == 1) {
    (void) pthread_detach(self);
    pthread_cancel(self);
  }
}

void threads_remove_async(struct threads* const threads, const uint32_t amount) {
  const uint32_t total = threads->used - amount;
  const pthread_t self = pthread_self();
  int close_ourselves = 0;
  for(uint32_t i = total; i < threads->used; ++i) {
    if(pthread_equal(threads->threads[i], self) == 0) {
      (void) pthread_detach(threads->threads[i]);
      (void) pthread_cancel(threads->threads[i]);
    } else {
      close_ourselves = 1;
    }
  }
  threads->used = total;
  if(close_ourselves == 1) {
    (void) pthread_detach(self);
    pthread_cancel(self);
  }
}

void threads_shutdown(struct threads* const threads) {
  threads_remove(threads, threads->used);
}

void threads_shutdown_async(struct threads* const threads) {
  threads_remove_async(threads, threads->used);
}

void threads_free(struct threads* const threads) {
  free(threads->threads);
  threads->threads = NULL;
  threads->used = 0;
  threads->size = 0;
}



#define pool ((struct thread_pool*) thread_pool_thread_data)

void thread_pool_thread(void* thread_pool_thread_data) {
  while(1) {
    thread_pool_work(pool);
  }
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
  struct thread_data* ptr;
  safe_execute(ptr = realloc(pool->queue, sizeof(struct thread_data) * new_size), ptr == NULL, ENOMEM);
  if(ptr == NULL) {
    return -1;
  }
  pool->queue = ptr;
  pool->size = new_size;
  return 0;
}

int thread_pool_resize(struct thread_pool* const pool, const uint32_t new_size) {
  thread_pool_lock(pool);
  if(thread_pool_resize_raw(pool, new_size) == -1) {
    thread_pool_unlock(pool);
    return -1;
  }
  thread_pool_unlock(pool);
  return 0;
}

int thread_pool_add_raw(struct thread_pool* const pool, void (*func)(void*), void* const data) {
  if(pool->used == pool->size && thread_pool_resize(pool, pool->size + 1) == -1) {
    return -1;
  }
  pool->queue[pool->used++] = (struct thread_data) { .func = func, .data = data };
  (void) sem_post(&pool->sem);
  return 0;
}

int thread_pool_add(struct thread_pool* const pool, void (*func)(void*), void* const data) {
  thread_pool_lock(pool);
  if(pool->used == pool->size && thread_pool_resize(pool, pool->size + 1) == -1) {
    thread_pool_unlock(pool);
    return -1;
  }
  pool->queue[pool->used++] = (struct thread_data) { .func = func, .data = data };
  thread_pool_unlock(pool);
  (void) sem_post(&pool->sem);
  return 0;
}

void thread_pool_try_work_raw(struct thread_pool* const pool) {
  struct thread_data data;
  if(pool->used != 0) {
    data.func = pool->queue->func;
    data.data = pool->queue->data;
    --pool->used;
    (void) memmove(pool->queue, pool->queue + 1, sizeof(struct thread_data) * pool->used);
    data.func(data.data);
  }
}

void thread_pool_try_work(struct thread_pool* const pool) {
  struct thread_data data;
  (void) pthread_mutex_lock(&pool->mutex);
  if(pool->used != 0) {
    data.func = pool->queue->func;
    data.data = pool->queue->data;
    --pool->used;
    (void) memmove(pool->queue, pool->queue + 1, sizeof(struct thread_data) * pool->used);
    (void) pthread_mutex_unlock(&pool->mutex);
    data.func(data.data);
  } else {
    (void) pthread_mutex_unlock(&pool->mutex);
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