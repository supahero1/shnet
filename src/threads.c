#include "threads.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>

#define thrd ((struct threads*) threads_thread_data)

static void threads_cleanup_routine(void* threads_thread_data) {
  const int flag = atomic_load(&thrd->flag);
  if(atomic_fetch_sub(&thrd->used, 1) == 1) {
    if(flag == threads_creation_error) {
      (void) pthread_barrier_destroy(thrd->barrier);
      free(thrd->barrier);
      atomic_store(&thrd->used, thrd->size);
    }
    if(thrd->on_stop != NULL) {
      thrd->on_stop(thrd);
    }
  }
}

static void* threads_thread(void* threads_thread_data) {
  (void) pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);
  (void) pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL);
  {
    sigset_t mask;
    (void) sigfillset(&mask);
    (void) pthread_sigmask(SIG_BLOCK, &mask, NULL);
  }
  pthread_cleanup_push(threads_cleanup_routine, threads_thread_data);
  (void) pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
  {
    int res = pthread_barrier_wait(thrd->barrier);
    if(res == PTHREAD_BARRIER_SERIAL_THREAD) {
      (void) pthread_barrier_destroy(thrd->barrier);
      free(thrd->barrier);
    }
  }
  if(atomic_fetch_add(&thrd->used, 1) + 1 == thrd->size && thrd->on_start != NULL) {
    thrd->on_start(thrd);
  }
  thrd->startup(thrd->data);
  pthread_cleanup_pop(1);
  return NULL;
}

#undef thrd

int threads_add(struct threads* const threads, const unsigned long amount, const int flag) {
  const unsigned long total = threads->size + amount;
  pthread_t* const ptr = malloc(sizeof(pthread_t) * total);
  if(ptr == NULL) {
    return threads_out_of_memory;
  }
  pthread_barrier_t* const barrier = malloc(sizeof(pthread_barrier_t));
  if(barrier == NULL) {
    free(ptr);
    return threads_out_of_memory;
  }
  int err = pthread_barrier_init(barrier, NULL, (unsigned int) amount);
  if(err != 0) {
    free(ptr);
    free(barrier);
    if(err == ENOMEM) {
      return threads_out_of_memory;
    }
    errno = err;
    return threads_failure;
  }
  pthread_attr_t* attr_ptr = NULL;
  if(flag == threads_detached) {
    pthread_attr_t attr;
    err = pthread_attr_init(&attr);
    if(err != 0) {
      free(ptr);
      (void) pthread_barrier_destroy(barrier);
      free(barrier);
      if(err == ENOMEM) {
        return threads_out_of_memory;
      }
      errno = err;
      return threads_failure;
    }
    (void) pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    attr_ptr = &attr;
  }
  const unsigned long old = threads->size;
  threads->size = total;
  threads->barrier = barrier;
  for(unsigned long i = old; i < total; ++i) {
    int err = pthread_create(ptr + i, attr_ptr, threads_thread, threads);
    if(err != 0) {
      atomic_store(&threads->flag, threads_creation_error);
      threads->size = old;
      if(i > old) {
        atomic_store(&threads->used, i - old);
        do {
          pthread_cancel(ptr[--i]);
        } while(i > old);
      } else {
        (void) pthread_barrier_destroy(barrier);
        free(barrier);
        atomic_store(&threads->used, old);
      }
      free(ptr);
      if(attr_ptr != NULL) {
        (void) pthread_attr_destroy(attr_ptr);
      }
      errno = err;
      return threads_failure;
    }
  }
  if(attr_ptr != NULL) {
    (void) pthread_attr_destroy(attr_ptr);
  }
  (void) memcpy(ptr, threads->threads, sizeof(pthread_t) * old);
  free(threads->threads);
  threads->threads = ptr;
  return threads_success;
}

int threads_remove(struct threads* const threads, const unsigned long amount) {
  const unsigned long total = threads->size - amount;
  pthread_t* ptr = NULL;
  pthread_t self = pthread_self();
  if(total != 0) {
    ptr = malloc(sizeof(pthread_t) * total);
    if(ptr == NULL) {
      return threads_out_of_memory;
    }
  }
  atomic_store(&threads->flag, threads_close);
  atomic_store(&threads->used, amount);
  int close_ourselves = 0;
  for(unsigned long i = total; i < threads->size; ++i) {
    if(pthread_equal(threads->threads[i], self) == 0) {
      pthread_cancel(threads->threads[i]);
    } else {
      close_ourselves = 1;
    }
  }
  if(total != 0) {
    (void) memcpy(ptr, threads->threads, sizeof(pthread_t) * total);
  }
  free(threads->threads);
  threads->threads = ptr;
  threads->size = total;
  if(close_ourselves == 1) {
    pthread_cancel(self);
  }
  return threads_success;
}

int threads_shutdown(struct threads* const threads) {
  return threads_remove(threads, threads->size);
}