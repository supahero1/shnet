#include "time.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>

uint64_t time_sec_to_ns(const uint64_t sec) {
  return sec * 1000000000;
}

uint64_t time_sec_to_us(const uint64_t sec) {
  return sec * 1000000;
}

uint64_t time_sec_to_ms(const uint64_t sec) {
  return sec * 1000;
}

uint64_t time_ms_to_ns(const uint64_t ms) {
  return ms * 1000000;
}

uint64_t time_ms_to_us(const uint64_t ms) {
  return ms * 1000;
}

uint64_t time_us_to_ns(const uint64_t us) {
  return us * 1000;
}

uint64_t time_ns_to_sec(const uint64_t ns) {
  return ns / 1000000000;
}

uint64_t time_us_to_sec(const uint64_t us) {
  return us / 1000000;
}

uint64_t time_ms_to_sec(const uint64_t ms) {
  return ms / 1000;
}

uint64_t time_ns_to_ms(const uint64_t ns) {
  return ns / 1000000;
}

uint64_t time_us_to_ms(const uint64_t us) {
  return us / 1000;
}

uint64_t time_ns_to_us(const uint64_t ns) {
  return ns / 1000;
}

uint64_t time_get_ns(const uint64_t ns) {
  struct timespec tp;
  (void) clock_gettime(CLOCK_REALTIME, &tp);
  return ns + tp.tv_sec * 1000000000 + tp.tv_nsec;
}

uint64_t time_get_us(const uint64_t us) {
  return time_get_ns(time_us_to_ns(us));
}

uint64_t time_get_ms(const uint64_t ms) {
  return time_get_ns(time_ms_to_ns(ms));
}

uint64_t time_get_sec(const uint64_t sec) {
  return time_get_ns(time_sec_to_ns(sec));
}

uint64_t time_get_time() {
  return time_get_ns(0);
}

static long time_timeout_compare(const void* a, const void* b) {
  const struct time_timeout* const x = a;
  const struct time_timeout* const y = b;
  if(x->time > y->time) {
    return 1;
  } else if(y->time > x->time) {
    return -1;
  } else {
    return 0;
  }
}

static long time_interval_compare(const void* a, const void* b) {
  const struct time_interval* const x = a;
  const struct time_interval* const y = b;
  const uint64_t t1 = x->base_time + x->interval * x->count;
  const uint64_t t2 = y->base_time + y->interval * y->count;
  if(t1 > t2) {
    return 1;
  } else if(t2 > t1) {
    return -1;
  } else {
    return 0;
  }
}

static void time_manager_thread(void*);

int time_manager(struct time_manager* const manager, void (*on_timer_expire)(struct time_manager*, struct time_timer, const int), const unsigned long timeouts_alloc_size, const unsigned long intervals_alloc_size) {
  *manager = (struct time_manager) {
    .timeouts = refheap(sizeof(struct time_timeout), heap_min, time_timeout_compare, timeouts_alloc_size),
    .intervals = refheap(sizeof(struct time_interval), heap_min, time_interval_compare, intervals_alloc_size),
    .on_timer_expire = on_timer_expire
  };
  int err = sem_init(&manager->work, 0, 0);
  if(err != 0) {
    errno = err;
    return time_failure;
  }
  err = sem_init(&manager->amount, 0, 0);
  if(err != 0) {
    (void) sem_destroy(&manager->work);
    errno = err;
    return time_failure;
  }
  err = pthread_mutex_init(&manager->mutex, NULL);
  if(err != 0) {
    (void) sem_destroy(&manager->work);
    (void) sem_destroy(&manager->amount);
    errno = err;
    return time_failure;
  }
  err = threads(&manager->thread);
  if(err != threads_success) {
    (void) sem_destroy(&manager->work);
    (void) sem_destroy(&manager->amount);
    (void) pthread_mutex_destroy(&manager->mutex);
    return time_failure;
  }
  manager->thread.func = time_manager_thread;
  manager->thread.data = manager;
  atomic_store(&manager->latest, 0);
  return time_success;
}

static int time_manager_set_latest(struct time_manager* const manager) {
  if(!refheap_is_empty(&manager->timeouts)) {
    if(!refheap_is_empty(&manager->intervals)) {
      struct time_timeout* const timeout = refheap_peak(&manager->timeouts, 0);
      struct time_interval* const interval = refheap_peak(&manager->intervals, 0);
      const uint64_t time = interval->base_time + interval->interval * interval->count;
      if(timeout->time > time) {
        atomic_store(&manager->latest, time | 1);
      } else {
        atomic_store(&manager->latest, timeout->time & 18446744073709551614U);
      }
    } else {
      atomic_store(&manager->latest, ((struct time_timeout*) refheap_peak(&manager->timeouts, 0))->time & 18446744073709551614U);
    }
  } else {
    if(!refheap_is_empty(&manager->intervals)) {
      struct time_interval* const interval = refheap_peak(&manager->intervals, 0);
      atomic_store(&manager->latest, (interval->base_time + interval->interval * interval->count) | 1);
    } else {
      atomic_store(&manager->latest, 0);
      return 1;
    }
  }
  return 0;
}

#define manager ((struct time_manager*) time_manager_thread_data)

static void time_manager_thread(void* time_manager_thread_data) {
  (void) pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL);
  {
    sigset_t mask;
    (void) sigfillset(&mask);
    (void) pthread_sigmask(SIG_BLOCK, &mask, NULL);
  }
  while(1) {
    check_for_timers:
    (void) sem_wait(&manager->amount);
    while(1) {
      uint64_t time = atomic_load(&manager->latest);
      (void) sem_timedwait(&manager->work, &(struct timespec){ .tv_sec = time / 1000000000, .tv_nsec = time % 1000000000 });
      time = atomic_load(&manager->latest);
      if(time_get_ns(0) >= time) {
        if(time == 0) {
          goto check_for_timers;
        }
        break;
      }
    }
    (void) pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);
    (void) pthread_mutex_lock(&manager->mutex);
    const uint64_t ltest = atomic_load(&manager->latest);
    if(ltest == 0) {
      (void) pthread_mutex_unlock(&manager->mutex);
    } else {
      if((ltest & 1) == 0) {
        struct time_timeout* latest = refheap_peak(&manager->timeouts, 0);
        struct time_timeout timeout = *latest;
        refheap_delete(&manager->timeouts, latest);
        (void) time_manager_set_latest(manager);
        (void) pthread_mutex_unlock(&manager->mutex);
        manager->on_timer_expire(manager, (struct time_timer) { .timeout = &timeout }, time_timeout);
      } else {
        struct time_interval* latest = refheap_peak(&manager->intervals, 0);
        struct time_interval interval = *latest;
        ++latest->count;
        refheap_down(&manager->intervals, refheap_ref_to_idx(&manager->intervals, latest));
        (void) sem_post(&manager->amount);
        (void) time_manager_set_latest(manager);
        (void) pthread_mutex_unlock(&manager->mutex);
        manager->on_timer_expire(manager, (struct time_timer) { .interval = &interval }, time_interval);
      }
    }
    (void) pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
    pthread_testcancel();
  }
}

#undef manager

int time_manager_start(struct time_manager* const manager) {
  return threads_add(&manager->thread, 1);
}

int time_manager_add_timeout(struct time_manager* const manager, const uint64_t time, void (*func)(void*), void* const data, struct time_timeout** const timeout) {
  (void) pthread_mutex_lock(&manager->mutex);
  {
    const int err = refheap_insert(&manager->timeouts, &((struct time_timeout) { .time = time, .func = func, .data = data }), (void**) timeout);
    if(err == heap_out_of_memory) {
      (void) pthread_mutex_unlock(&manager->mutex);
      return time_out_of_memory;
    }
  }
  if(time_manager_set_latest(manager) == 0) {
    (void) sem_post(&manager->work);
  }
  (void) sem_post(&manager->amount);
  (void) pthread_mutex_unlock(&manager->mutex);
  return time_success;
}

int time_manager_add_interval(struct time_manager* const manager, const uint64_t base_time, const uint64_t intrvl, void (*func)(void*), void* const data, struct time_interval** const interval, const unsigned long instant) {
  (void) pthread_mutex_lock(&manager->mutex);
  {
    const int err = refheap_insert(&manager->intervals, &((struct time_interval) { .base_time = base_time, .interval = intrvl, .count = instant, .func = func, .data = data }), (void**) interval);
    if(err == heap_out_of_memory) {
      (void) pthread_mutex_unlock(&manager->mutex);
      return time_out_of_memory;
    }
  }
  if(time_manager_set_latest(manager) == 0) {
    (void) sem_post(&manager->work);
  }
  (void) sem_post(&manager->amount);
  (void) pthread_mutex_unlock(&manager->mutex);
  return time_success;
}

void time_manager_cancel_timeout(struct time_manager* const manager, struct time_timeout* const timeout) {
  (void) pthread_mutex_lock(&manager->mutex);
  refheap_delete(&manager->timeouts, timeout);
  (void) time_manager_set_latest(manager);
  (void) sem_post(&manager->work);
  (void) pthread_mutex_unlock(&manager->mutex);
}

void time_manager_cancel_interval(struct time_manager* const manager, struct time_interval* const interval) {
  (void) pthread_mutex_lock(&manager->mutex);
  refheap_delete(&manager->intervals, interval);
  (void) time_manager_set_latest(manager);
  (void) sem_post(&manager->work);
  (void) pthread_mutex_unlock(&manager->mutex);
}

void time_manager_stop(struct time_manager* const manager) {
  threads_shutdown(&manager->thread);
}

void time_manager_free(struct time_manager* const manager) {
  (void) sem_destroy(&manager->work);
  (void) sem_destroy(&manager->amount);
  (void) pthread_mutex_destroy(&manager->mutex);
  refheap_free(&manager->timeouts);
  refheap_free(&manager->intervals);
  threads_free(&manager->thread);
}