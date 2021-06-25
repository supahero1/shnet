#include "threads.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>

int threads(struct threads* const threads) {
  (void) memset(threads, 0, sizeof(struct threads));
  int err = sem_init(&threads->sem, 0, 0);
  if(err != 0) {
    errno = err;
    return threads_failure;
  }
  return threads_success;
}

#define thrd ((struct threads*) threads_thread_data)

static void* threads_thread(void* threads_thread_data) {
  (void) pthread_barrier_wait(thrd->barrier);
  if(atomic_fetch_sub(&thrd->togo, 1) == 1) {
    (void) pthread_barrier_destroy(thrd->barrier);
    free(thrd->barrier);
    (void) sem_post(&thrd->sem);
  }
  thrd->func(thrd->data);
  return NULL;
}

#undef thrd

int threads_resize(struct threads* const threads, const unsigned new_size) {
  pthread_t* const ptr = realloc(threads->threads, sizeof(pthread_t) * new_size);
  if(ptr == NULL) {
    return threads_out_of_memory;
  }
  threads->threads = ptr;
  threads->size = new_size;
  return threads_success;
}

int threads_add(struct threads* const threads, const unsigned amount) {
  const unsigned total = threads->used + amount;
  if(total > threads->size) {
    int err = threads_resize(threads, total);
    if(err != threads_success) {
      return err;
    }
  }
  pthread_barrier_t* const barrier = malloc(sizeof(pthread_barrier_t));
  if(barrier == NULL) {
    return threads_out_of_memory;
  }
  int err = pthread_barrier_init(barrier, NULL, amount);
  if(err != 0) {
    free(barrier);
    if(err == ENOMEM) {
      return threads_out_of_memory;
    }
    errno = err;
    return threads_failure;
  }
  threads->barrier = barrier;
  atomic_store(&threads->togo, amount);
  for(unsigned i = threads->used; i < total; ++i) {
    int err = pthread_create(threads->threads + i, NULL, threads_thread, threads);
    if(err != 0) {
      if(i > threads->size) {
        const unsigned old = threads->used;
        threads->used = i;
        (void) threads_remove(threads, i - threads->used);
        threads->used = old;
      } else {
        (void) pthread_barrier_destroy(barrier);
        free(barrier);
      }
      errno = err;
      return threads_failure;
    }
  }
  threads->used = total;
  (void) sem_wait(&threads->sem);
  return threads_success;
}

void threads_remove(struct threads* const threads, const unsigned amount) {
  const unsigned total = threads->used - amount;
  const pthread_t self = pthread_self();
  atomic_store(&threads->togo, amount);
  int close_ourselves = 0;
  for(unsigned i = total; i < threads->used; ++i) {
    if(pthread_equal(threads->threads[i], self) == 0) {
      (void) pthread_cancel(threads->threads[i]);
    } else {
      close_ourselves = 1;
    }
  }
  for(unsigned i = total; i < threads->used; ++i) {
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