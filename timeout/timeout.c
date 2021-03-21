/*
   Copyright 2020-2021 shädam

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at

       http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License.
*/

#include "timeout.h"

#include <time.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <stdbool.h>

uint64_t GetTime(const uint64_t nanoseconds) {
  struct timespec tp = { .tv_sec = 0, .tv_nsec = 0 };
  (void) clock_gettime(CLOCK_MONOTONIC, &tp);
  return nanoseconds + tp.tv_sec * 1000000000 + tp.tv_nsec;
}

struct Timeout Timeout() {
  return (struct Timeout) {
    .heap = NULL,
    .timeouts = 1,
    .max_timeouts = 1
  };
}

__nonnull((1))
static void TimeoutHeap_insert(struct Timeout* const timeout, const struct TimeoutObject obj) {
  uint32_t idx = timeout->timeouts++;
  uint32_t parent = idx >> 1;
  if(idx == 1) {
    timeout->heap[1] = obj;
  } else {
    while(timeout->heap[parent].time > obj.time) {
      timeout->heap[idx] = timeout->heap[parent];
      if(parent == 1) {
        timeout->heap[1] = obj;
        return;
      } else {
        idx = parent;
        parent >>= 1;
      }
    }
    timeout->heap[idx] = obj;
  }
}

__nonnull((1))
static struct TimeoutObject TimeoutHeap_pop(struct Timeout* const timeout) {
  const struct TimeoutObject obj = timeout->heap[1];
  uint32_t idx = 1;
  --timeout->timeouts;
  if((idx << 1) <= timeout->timeouts) {
    do {
      if((idx << 1) + 1 <= timeout->timeouts && timeout->heap[(idx << 1) + 1].time < timeout->heap[timeout->timeouts].time && timeout->heap[(idx << 1) + 1].time < timeout->heap[idx << 1].time) {
        timeout->heap[idx] = timeout->heap[(idx << 1) + 1];
        idx = (idx << 1) + 1;
      } else if(timeout->heap[idx << 1].time < timeout->heap[timeout->timeouts].time) {
        timeout->heap[idx] = timeout->heap[idx << 1];
        idx <<= 1;
      } else {
        break;
      }
    } while((idx << 1) <= timeout->timeouts);
    timeout->heap[idx] = timeout->heap[timeout->timeouts];
  }
  return obj;
}

int AddTimeout(struct Timeout* const timeout, const struct TimeoutObject* const work, const uint32_t amount) {
  (void) pthread_mutex_lock(&timeout->mutex);
  if(timeout->max_timeouts - timeout->timeouts < amount) {
    struct TimeoutObject* ptr = realloc(timeout->heap, sizeof(struct TimeoutObject) * (timeout->timeouts + amount));
    if(ptr == NULL) {
      (void) pthread_mutex_unlock(&timeout->mutex);
      return ENOMEM;
    }
    timeout->heap = ptr;
    timeout->max_timeouts = timeout->timeouts + amount;
  }
  uint32_t i = 0;
  for(; i < amount; ++i) {
    TimeoutHeap_insert(timeout, work[i]);
  }
  timeout->timeouts += amount;
  (void) pthread_mutex_unlock(&timeout->mutex);
  (void) pthread_sigqueue(timeout->worker, SIGRTMAX, (union sigval) { .sival_int = 0 });
  return 0;
}

void TimeoutCleanup(struct Timeout* const timeout) {
  (void) pthread_mutex_lock(&timeout->mutex);
  if(timeout->timeouts != timeout->max_timeouts) {
    timeout->heap = realloc(timeout->heap, sizeof(struct TimeoutObject) * timeout->timeouts);
    timeout->max_timeouts = timeout->timeouts;
  }
  (void) pthread_mutex_unlock(&timeout->mutex);
}

#define timeout ((struct Timeout*) t)

__nonnull((1))
static void TimeoutWorkerCleanup(void* t) {
  (void) pthread_mutex_destroy(&timeout->mutex);
  if(timeout->onclear != NULL) {
    timeout->onclear(timeout);
  }
  if((timeout->clear_mode == TIME_DEPENDS && atomic_load(&timeout->clean_work) == TIME_ALWAYS) || timeout->clear_mode == TIME_ALWAYS) {
    timeout->timeouts = 1;
    timeout->max_timeouts = 1;
    free(timeout->heap);
    timeout->heap = NULL;
  }
  atomic_store(&timeout->state, TIME_DEAD);
}

__nonnull((1))
static void* TimeoutThread(void* t) {
  struct timespec t;
  struct TimeoutObject obj;
  void (*callback)(void*);
  void* data;
  sigset_t mask;
  (void) sigfillset(&mask);
  (void) sigdelset(&mask, SIGRTMAX);
  (void) pthread_sigmask(SIG_BLOCK, &mask, NULL);
  (void) sigemptyset(&mask);
  (void) sigaddset(&mask, SIGRTMAX);
  atomic_store(&timeout->state, TIME_ALIVE);
  if(timeout->onready != NULL) {
    timeout->onready(timeout);
  }
  while(1) {
    (void) sigsuspend(&mask);
    (void) pthread_mutex_lock(&timeout->mutex);
    while(timeout->timeouts != 1) {
      timeout->heap[0] = TimeoutHeap_pop(timeout);
      callback = timeout->heap[0].func;
      data = timeout->heap[0].data;
      if(atomic_load(&timeout->clean_work) == TIME_ALWAYS && timeout->max_timeouts != timeout->timeouts) {
        timeout->heap = realloc(timeout->heap, sizeof(struct TimeoutObject) * timeout->timeouts);
        timeout->max_timeouts = timeout->timeouts;
      }
      t = (struct timespec) { .tv_sec = timeout->heap[0].time / 1000000000, .tv_nsec = timeout->heap[0].time % 1000000000 };
      (void) pthread_mutex_unlock(&timeout->mutex);
      // 
      while(clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &t, NULL) != 0) {
        (void) pthread_mutex_lock(&timeout->mutex);
        atomic_store(&timeout->state, TIME_RUNNING);
        if(timeout->heap[1].time < timeout->heap[0].time) {
          obj = TimeoutHeap_pop(timeout);
          TimeoutHeap_insert(timeout, timeout->heap[0]);
          timeout->heap[0] = obj;
          callback = timeout->heap[0].func;
          data = timeout->heap[0].data;
          if(atomic_load(&timeout->clean_work) == TIME_ALWAYS) {
            timeout->heap = realloc(timeout->heap, sizeof(struct TimeoutObject) * timeout->timeouts);
            timeout->max_timeouts = timeout->timeouts;
          }
          (void) pthread_mutex_unlock(&timeout->mutex);
          t = (struct timespec) { .tv_sec = timeout->heap[0].time / 1000000000, .tv_nsec = timeout->heap[0].time % 1000000000 };
        } else {
          (void) pthread_mutex_unlock(&timeout->mutex);
        }
      }
      atomic_store(&timeout->state, TIME_RUNNING);
      callback(data);
    }
    (void) pthread_mutex_unlock(&timeout->mutex);
    (void) pthread_sigmask(SIG_BLOCK, &mask, NULL);
    atomic_store(&timeout->state, TIME_SLEEPING);
    (void) sigsuspend(&rt_mask);
    (void) pthread_sigmask(SIG_UNBLOCK, &mask, NULL);
    atomic_store(&timeout->state, TIME_RUNNING);
  }
  return NULL;
}

#undef timeout

void StopTimeoutThread(struct Timeout* const timeout, const uint32_t clear_mode) {
  timeout->clear_mode = clear_mode;
  atomic_store(&timeout->state, TIME_DEAD);
  (void) pthread_sigqueue(timeout->worker, SIGRTMAX, (union sigval) { .sival_ptr = NULL });
}

int StartTimeoutThread(struct Timeout* const timeout, const uint32_t clean_work) {
  atomic_store(&timeout->clean_work, clean_work);
  pthread_attr_t* attr;
  if(pthread_attr_init(attr) != 0) {
    return ENOMEM;
  }
  int err = pthread_mutex_init(&timeout->mutex, NULL);
  if(err != 0) {
    (void) pthread_attr_destroy(attr);
    return err;
  }
  (void) pthread_attr_setschedpolicy(attr, SCHED_FIFO);
  err = pthread_create(&timeout->worker, attr, TimeoutThread, timeout);
  if(err != 0) {
    (void) pthread_attr_destroy(attr);
    (void) pthread_mutex_destroy(&timeout->mutex);
  }
  return err;
}