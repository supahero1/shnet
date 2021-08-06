#include "threads.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>

int threads(struct threads* const threads) {
  int err = sem_init(&threads->sem, 0, 0);
  if(err != 0) {
    errno = err;
    return -1;
  }
  return 0;
}

#define thrd ((struct threads*) threads_thread_data)

static void* threads_thread(void* threads_thread_data) {
  (void) pthread_barrier_wait(&thrd->barrier);
  if(atomic_fetch_sub(&thrd->togo, 1) == 1) {
    (void) pthread_barrier_destroy(&thrd->barrier);
    (void) sem_post(&thrd->sem);
  }
  thrd->func(thrd->data);
  return NULL;
}

#undef thrd

int threads_resize(struct threads* const threads, const uint32_t new_size) {
  pthread_t* const ptr = realloc(threads->threads, sizeof(pthread_t) * new_size);
  if(ptr == NULL) {
    return -1;
  }
  threads->threads = ptr;
  threads->size = new_size;
  return 0;
}

int threads_add(struct threads* const threads, const uint32_t amount) {
  const uint32_t total = threads->used + amount;
  if(total > threads->size) {
    if(threads_resize(threads, total) != 0) {
      return -1;
    }
  }
  int err = pthread_barrier_init(&threads->barrier, NULL, amount);
  if(err != 0) {
    errno = err;
    return -1;
  }
  atomic_store(&threads->togo, amount);
  for(uint32_t i = threads->used; i < total; ++i) {
    int err = pthread_create(threads->threads + i, NULL, threads_thread, threads);
    if(err != 0) {
      if(i > threads->used) {
        const uint32_t old = threads->used;
        threads->used = i;
        (void) threads_remove(threads, i - threads->used);
        threads->used = old;
      } else {
        (void) pthread_barrier_destroy(&threads->barrier);
      }
      errno = err;
      return -1;
    }
  }
  threads->used = total;
  (void) sem_wait(&threads->sem);
  return 0;
}

void threads_remove(struct threads* const threads, const uint32_t amount) {
  const uint32_t total = threads->used - amount;
  const pthread_t self = pthread_self();
  atomic_store(&threads->togo, amount);
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

void threads_shutdown(struct threads* const threads) {
  threads_remove(threads, threads->used);
}

void threads_free(struct threads* const threads) {
  free(threads->threads);
  (void) sem_destroy(&threads->sem);
}



int thread_start(struct thread* const thread, void* (*func)(void*), void* const data) {
  const int err = pthread_create(&thread->thread, NULL, func, data);
  if(err != 0) {
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