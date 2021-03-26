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

#include "time.h"

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
    .onstart = NULL,
    .onstop = NULL,
    .heap = NULL,
    .timeouts = 1,
    .max_timeouts = 1
  };
}

static void TimeoutHeapInsert(struct Timeout* const timeout, const struct TimeoutObject obj) {
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

static void TimeoutHeapPop(struct Timeout* const timeout) {
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
}

int AddTimeout(struct Timeout* const timeout, const struct TimeoutObject* const work, const uint32_t amount) {
  int locked = pthread_mutex_trylock(&timeout->mutex);
  uint32_t old = timeout->timeouts;
  if(timeout->max_timeouts - timeout->timeouts < amount) {
    struct TimeoutObject* ptr = realloc(timeout->heap, sizeof(struct TimeoutObject) * (timeout->timeouts + amount));
    if(ptr == NULL) {
      if(locked != EBUSY) {
        (void) pthread_mutex_unlock(&timeout->mutex);
      }
      return ENOMEM;
    }
    timeout->heap = ptr;
    timeout->max_timeouts = timeout->timeouts + amount;
  }
  uint64_t time = timeout->heap[1].time;
  uint32_t i = 0;
  for(; i < amount; ++i) {
    TimeoutHeapInsert(timeout, work[i]);
  }
  if(timeout->heap[1].time != time || old == 1) {
    if(locked != EBUSY) {
      (void) pthread_mutex_unlock(&timeout->mutex);
    }
    (void) pthread_sigqueue(timeout->worker, SIGRTMAX, (union sigval) { .sival_ptr = timeout });
  } else if(locked != EBUSY) {
    (void) pthread_mutex_unlock(&timeout->mutex);
  }
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

#define timeout ((struct Timeout*) info->si_value.sival_ptr)

static void TimeoutWorkerHandler(int sig) {}

#undef timeout
#define timeout ((struct Timeout*) t)
#define LOCK                                   \
(void) pthread_sigmask(SIG_BLOCK, &mask, NULL); \
(void) pthread_mutex_lock(&timeout->mutex)
#define UNLOCK                                    \
do {                                               \
  (void) pthread_mutex_unlock(&timeout->mutex);     \
  if((atomic_load(&timeout->clean_work) & 1) == 1) { \
    goto terminate;                                   \
  }                                                    \
  (void) pthread_sigmask(SIG_UNBLOCK, &mask, NULL);     \
} while(0)

static void* TimeoutThread(void* t) {
  struct timespec time;
  sigset_t mask;
  (void) sigfillset(&mask);
  (void) sigdelset(&mask, SIGRTMAX);
  (void) pthread_sigmask(SIG_BLOCK, &mask, NULL);
  (void) sigemptyset(&mask);
  (void) sigaddset(&mask, SIGRTMAX);
  (void) sigaction(SIGRTMAX, &((struct sigaction) {
    .sa_flags = 0,
    .sa_handler = TimeoutWorkerHandler
  }), NULL);
  if(timeout->onstart != NULL) {
    timeout->onstart(timeout);
  }
  (void) pthread_mutex_lock(&timeout->mutex);
  if(timeout->timeouts != 1) {
    goto skip;
  }
  (void) pthread_mutex_unlock(&timeout->mutex);
  while(1) {
    (void) sigsuspend(&mask);
    LOCK;
    if(timeout->timeouts == 1) {
      UNLOCK;
      continue;
    }
    while(timeout->timeouts != 1) {
      skip:
      timeout->heap[0] = timeout->heap[1];
      time = (struct timespec) { .tv_sec = timeout->heap[0].time / 1000000000, .tv_nsec = timeout->heap[0].time % 1000000000 };
      UNLOCK;
      while(clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &time, NULL) != 0) {
        LOCK;
        timeout->heap[0] = timeout->heap[1];
        time = (struct timespec) { .tv_sec = timeout->heap[0].time / 1000000000, .tv_nsec = timeout->heap[0].time % 1000000000 };
        UNLOCK;
      }
      LOCK;
      (void) TimeoutHeapPop(timeout);
      if(atomic_load(&timeout->clean_work) == TIME_ALWAYS) {
        timeout->heap = realloc(timeout->heap, sizeof(struct TimeoutObject) * timeout->timeouts);
        timeout->max_timeouts = timeout->timeouts;
      }
      timeout->heap[0].func(timeout->heap[0].data);
      UNLOCK;
    }
  }
  terminate:
  (void) pthread_sigmask(SIG_BLOCK, &mask, NULL);
  if(timeout->onstop != NULL) {
    timeout->onstop(timeout);
  }
  (void) pthread_mutex_destroy(&timeout->mutex);
  if(timeout->clear_mode == TIME_ALWAYS || (timeout->clear_mode == TIME_DEPENDS && atomic_load(&timeout->clean_work) == TIME_ALWAYS)) {
    timeout->timeouts = 1;
    timeout->max_timeouts = 1;
    free(timeout->heap);
    timeout->heap = NULL;
  }
  return NULL;
}

#undef timeout

void StopTimeoutThread(struct Timeout* const timeout, const uint32_t clear_mode) {
  atomic_store(&timeout->clean_work, atomic_load(&timeout->clean_work) | 1);
  atomic_store(&timeout->clear_mode, clear_mode);
  (void) pthread_sigqueue(timeout->worker, SIGRTMAX, (union sigval) { .sival_ptr = NULL });
}

int StartTimeoutThread(struct Timeout* const timeout, const uint32_t clean_work) {
  atomic_store(&timeout->clean_work, clean_work);
  int err = pthread_mutex_init(&timeout->mutex, NULL);
  if(err != 0) {
    return err;
  }
  err = pthread_create(&timeout->worker, NULL, TimeoutThread, timeout);
  if(err != 0) {
    (void) pthread_mutex_destroy(&timeout->mutex);
  }
  return err;
}