#include "time.h"
#include "error.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>

uint64_t time_sec_to_ms(const uint64_t sec) {
  return sec * 1000;
}

uint64_t time_sec_to_us(const uint64_t sec) {
  return sec * 1000000;
}

uint64_t time_sec_to_ns(const uint64_t sec) {
  return sec * 1000000000;
}


uint64_t time_ms_to_sec(const uint64_t ms) {
  return ms / 1000;
}

uint64_t time_ms_to_us(const uint64_t ms) {
  return ms * 1000;
}

uint64_t time_ms_to_ns(const uint64_t ms) {
  return ms * 1000000;
}


uint64_t time_us_to_sec(const uint64_t us) {
  return us / 1000000;
}

uint64_t time_us_to_ms(const uint64_t us) {
  return us / 1000;
}

uint64_t time_us_to_ns(const uint64_t us) {
  return us * 1000;
}


uint64_t time_ns_to_sec(const uint64_t ns) {
  return ns / 1000000000;
}

uint64_t time_ns_to_ms(const uint64_t ns) {
  return ns / 1000000;
}

uint64_t time_ns_to_us(const uint64_t ns) {
  return ns / 1000;
}


uint64_t time_get_sec(const uint64_t sec) {
  return time_get_ns(time_sec_to_ns(sec));
}

uint64_t time_get_ms(const uint64_t ms) {
  return time_get_ns(time_ms_to_ns(ms));
}

uint64_t time_get_us(const uint64_t us) {
  return time_get_ns(time_us_to_ns(us));
}

uint64_t time_get_ns(const uint64_t ns) {
  struct timespec tp;
  (void) clock_gettime(CLOCK_REALTIME, &tp);
  return ns + tp.tv_sec * 1000000000 + tp.tv_nsec;
}

uint64_t time_get_time() {
  return time_get_ns(0);
}


static int time_timeout_compare(const void* a, const void* b) {
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

static int time_interval_compare(const void* a, const void* b) {
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

int time_manager(struct time_manager* const manager) {
  manager->timeouts.sign = heap_min;
  manager->timeouts.compare = time_timeout_compare;
  manager->timeouts.item_size = sizeof(struct time_timeout) + sizeof(void**);
  manager->timeouts.used = manager->timeouts.item_size;
  manager->timeouts.size = manager->timeouts.item_size;
  
  manager->intervals.sign = heap_min;
  manager->intervals.compare = time_interval_compare;
  manager->intervals.item_size = sizeof(struct time_interval) + sizeof(void**);
  manager->intervals.used = manager->intervals.item_size;
  manager->intervals.size = manager->intervals.item_size;
  
  int err;
  safe_execute(err = sem_init(&manager->work, 0, 0), err != 0, err);
  if(err != 0) {
    goto err;
  }
  safe_execute(err = sem_init(&manager->amount, 0, 0), err != 0, err);
  if(err != 0) {
    goto err_sem;
  }
  safe_execute(err = pthread_mutex_init(&manager->mutex, NULL), err != 0, err);
  if(err != 0) {
    goto err_sem2;
  }
  return 0;
  
  err_sem2:
  (void) sem_destroy(&manager->amount);
  err_sem:
  (void) sem_destroy(&manager->work);
  err:
  errno = err;
  return -1;
}

static int time_manager_set_latest(struct time_manager* const manager) {
  const uint64_t old = atomic_load_explicit(&manager->latest, memory_order_acquire);
  uint64_t new;
  if(!refheap_is_empty(&manager->timeouts)) {
    if(!refheap_is_empty(&manager->intervals)) {
      const struct time_timeout* const timeout = refheap_peak_rel(&manager->timeouts, 1);
      const struct time_interval* const interval = refheap_peak_rel(&manager->intervals, 1);
      const uint64_t time = interval->base_time + interval->interval * interval->count;
      if(timeout->time > time) {
        new = time | 1;
      } else {
        new = timeout->time & (UINT64_MAX - 1);
      }
    } else {
      const struct time_timeout* const timeout = refheap_peak_rel(&manager->timeouts, 1);
      new = timeout->time & (UINT64_MAX - 1);
    }
  } else {
    if(!refheap_is_empty(&manager->intervals)) {
      const struct time_interval* const interval = refheap_peak_rel(&manager->intervals, 1);
      new = (interval->base_time + interval->interval * interval->count) | 1;
    } else {
      new = 0;
    }
  }
  atomic_store_explicit(&manager->latest, new, memory_order_release);
  if(new != old && new != 0) {
    return 1;
  }
  return 0;
}

#define manager ((struct time_manager*) time_manager_thread_data)

static void time_manager_thread(void* time_manager_thread_data) {
  uint64_t glob_time;
  while(1) {
    check_for_timers:
    (void) sem_wait(&manager->amount);
    while(1) {
      uint64_t time = atomic_load_explicit(&manager->latest, memory_order_acquire);
      (void) sem_timedwait(&manager->work, &(struct timespec){ .tv_sec = time_ns_to_sec(time), .tv_nsec = time % 1000000000 });
      time = atomic_load_explicit(&manager->latest, memory_order_acquire);
      glob_time = time_get_time();
      if(glob_time >= time) {
        if(time == 0) {
          goto check_for_timers;
        }
        break;
      }
    }
    time_manager_lock(manager);
    const uint64_t ltest = atomic_load_explicit(&manager->latest, memory_order_acquire);
    if(ltest == 0 || glob_time < ltest) {
      time_manager_unlock(manager);
    } else {
      if((ltest & 1) == 0) {
        struct time_timeout* latest = refheap_peak_rel(&manager->timeouts, 1);
        struct time_reference* const ref = *(struct time_reference**)((char*) latest - sizeof(void**));
        if(ref != NULL) {
          ref->timer = 1;
        }
        struct time_timeout timeout = *latest;
        refheap_delete(&manager->timeouts, manager->timeouts.item_size);
        (void) time_manager_set_latest(manager);
        time_manager_unlock(manager);
        timeout.func(timeout.data);
      } else {
        struct time_interval* latest = refheap_peak_rel(&manager->intervals, 1);
        struct time_interval interval = *latest;
        ++latest->count;
        refheap_down(&manager->intervals, manager->intervals.item_size);
        (void) sem_post(&manager->amount);
        (void) time_manager_set_latest(manager);
        time_manager_unlock(manager);
        interval.func(interval.data);
      }
    }
  }
}

#undef manager

int time_manager_start(struct time_manager* const manager) {
  return thread_start(&manager->thread, time_manager_thread, manager);
}

void time_manager_lock(struct time_manager* const manager) {
  (void) pthread_mutex_lock(&manager->mutex);
}

void time_manager_unlock(struct time_manager* const manager) {
  (void) pthread_mutex_unlock(&manager->mutex);
}

void time_manager_stop(struct time_manager* const manager) {
  thread_stop(&manager->thread);
}

void time_manager_stop_async(struct time_manager* const manager) {
  thread_stop_async(&manager->thread);
}

void time_manager_free(struct time_manager* const manager) {
  (void) sem_destroy(&manager->work);
  (void) sem_destroy(&manager->amount);
  (void) pthread_mutex_destroy(&manager->mutex);
  refheap_free(&manager->timeouts);
  refheap_free(&manager->intervals);
}



int time_manager_resize_timeouts_raw(struct time_manager* const manager, const uint64_t new_size) {
  return refheap_resize(&manager->timeouts, manager->timeouts.item_size * (new_size + 1));
}

int time_manager_resize_timeouts(struct time_manager* const manager, const uint64_t new_size) {
  time_manager_lock(manager);
  const int ret = time_manager_resize_timeouts_raw(manager, new_size);
  time_manager_unlock(manager);
  return ret;
}

int time_manager_add_timeout_raw(struct time_manager* const manager, const uint64_t time, void (*func)(void*), void* const data, struct time_reference* const ref) {
  if(refheap_insert(&manager->timeouts, &(struct time_timeout){ time, func, data }, (uint64_t*) ref) == -1) {
    return -1;
  }
  if(time_manager_set_latest(manager)) {
    (void) sem_post(&manager->work);
  }
  (void) sem_post(&manager->amount);
  return 0;
}

int time_manager_add_timeout(struct time_manager* const manager, const uint64_t time, void (*func)(void*), void* const data, struct time_reference* const ref) {
  time_manager_lock(manager);
  const int ret = time_manager_add_timeout_raw(manager, time, func, data, ref);
  time_manager_unlock(manager);
  return ret;
}

int time_manager_cancel_timeout_raw(struct time_manager* const manager, struct time_reference* const ref) {
  if(ref->timer != 0 && (ref->timer & 1) == 0) {
    refheap_delete(&manager->timeouts, ref->timer);
    ref->timer = 1;
    if(time_manager_set_latest(manager)) {
      (void) sem_post(&manager->work);
    }
    return 1;
  }
  return 0;
}

int time_manager_cancel_timeout(struct time_manager* const manager, struct time_reference* const ref) {
  time_manager_lock(manager);
  const int ret = time_manager_cancel_timeout_raw(manager, ref);
  time_manager_unlock(manager);
  return ret;
}

struct time_timeout* time_manager_open_timeout_raw(struct time_manager* const manager, struct time_reference* const ref) {
  if(ref->timer != 0 && (ref->timer & 1) == 0) {
    struct time_timeout* const timeout = refheap_peak(&manager->timeouts, ref->timer);
    ref->last_time = timeout->time;
    return timeout;
  }
  return NULL;
}

struct time_timeout* time_manager_open_timeout(struct time_manager* const manager, struct time_reference* const ref) {
  time_manager_lock(manager);
  if(ref->timer != 0 && (ref->timer & 1) == 0) {
    struct time_timeout* const timeout = refheap_peak(&manager->timeouts, ref->timer);
    ref->last_time = timeout->time;
    return timeout;
  }
  time_manager_unlock(manager);
  return NULL;
}

void time_manager_close_timeout_raw(struct time_manager* const manager, struct time_reference* const ref) {
  const struct time_timeout* const timeout = refheap_peak(&manager->timeouts, ref->timer);
  if(timeout->time > ref->last_time) {
    refheap_down(&manager->timeouts, ref->timer);
  } else if(timeout->time < ref->last_time) {
    refheap_up(&manager->timeouts, ref->timer);
  }
  if(time_manager_set_latest(manager)) {
    (void) sem_post(&manager->work);
  }
}

void time_manager_close_timeout(struct time_manager* const manager, struct time_reference* const ref) {
  time_manager_close_timeout_raw(manager, ref);
  time_manager_unlock(manager);
}



int time_manager_resize_intervals_raw(struct time_manager* const manager, const uint64_t new_size) {
  return refheap_resize(&manager->intervals, manager->intervals.item_size * (new_size + 1));
}

int time_manager_resize_intervals(struct time_manager* const manager, const uint64_t new_size) {
  time_manager_lock(manager);
  const int ret = time_manager_resize_intervals_raw(manager, new_size);
  time_manager_unlock(manager);
  return ret;
}

int time_manager_add_interval_raw(struct time_manager* const manager, const uint64_t base_time, const uint64_t interval, void (*func)(void*), void* const data, struct time_reference* const ref) {
  if(refheap_insert(&manager->intervals, &(struct time_interval){ base_time, interval, 0, func, data }, (uint64_t*) ref) == -1) {
    return -1;
  }
  if(time_manager_set_latest(manager)) {
    (void) sem_post(&manager->work);
  }
  (void) sem_post(&manager->amount);
  return 0;
}

int time_manager_add_interval(struct time_manager* const manager, const uint64_t base_time, const uint64_t interval, void (*func)(void*), void* const data, struct time_reference* const ref) {
  time_manager_lock(manager);
  const int ret = time_manager_add_interval_raw(manager, base_time, interval, func, data, ref);
  time_manager_unlock(manager);
  return ret;
}

int time_manager_cancel_interval_raw(struct time_manager* const manager, struct time_reference* const ref) {
  if(ref->timer != 0 && (ref->timer & 1) == 0) {
    refheap_delete(&manager->intervals, ref->timer);
    ref->timer = 1;
    if(time_manager_set_latest(manager)) {
      (void) sem_post(&manager->work);
    }
    return 1;
  }
  return 0;
}

int time_manager_cancel_interval(struct time_manager* const manager, struct time_reference* const ref) {
  time_manager_lock(manager);
  const int ret = time_manager_cancel_interval_raw(manager, ref);
  time_manager_unlock(manager);
  return ret;
}

struct time_interval* time_manager_open_interval_raw(struct time_manager* const manager, struct time_reference* const ref) {
  if(ref->timer != 0 && (ref->timer & 1) == 0) {
    struct time_interval* const interval = refheap_peak(&manager->intervals, ref->timer);
    ref->last_time = interval->base_time + interval->interval * interval->count;
    return interval;
  }
  return NULL;
}

struct time_interval* time_manager_open_interval(struct time_manager* const manager, struct time_reference* const ref) {
  time_manager_lock(manager);
  if(ref->timer != 0 && (ref->timer & 1) == 0) {
    struct time_interval* const interval = refheap_peak(&manager->intervals, ref->timer);
    ref->last_time = interval->base_time + interval->interval * interval->count;
    return interval;
  }
  time_manager_unlock(manager);
  return NULL;
}

void time_manager_close_interval_raw(struct time_manager* const manager, struct time_reference* const ref) {
  const struct time_interval* const interval = refheap_peak(&manager->intervals, ref->timer);
  const uint64_t time = interval->base_time + interval->interval * interval->count;
  if(time > ref->last_time) {
    refheap_down(&manager->intervals, ref->timer);
  } else if(time < ref->last_time) {
    refheap_up(&manager->intervals, ref->timer);
  }
  if(time_manager_set_latest(manager)) {
    (void) sem_post(&manager->work);
  }
}

void time_manager_close_interval(struct time_manager* const manager, struct time_reference* const ref) {
  time_manager_close_interval_raw(manager, ref);
  time_manager_unlock(manager);
}