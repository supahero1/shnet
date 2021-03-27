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
#include <stdio.h>

uint64_t GetTime(const uint64_t nanoseconds) {
  struct timespec tp = { .tv_sec = 0, .tv_nsec = 0 };
  (void) clock_gettime(CLOCK_REALTIME, &tp);
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

static void TimeoutHeapPop(struct Timeout* const t) {
  uint32_t idx = 1;
  --t->timeouts;
  t->heap[0] = t->heap[1];
  if((idx << 1) <= t->timeouts) {
    do {
      if((idx << 1) + 1 <= t->timeouts && t->heap[(idx << 1) + 1].time < t->heap[t->timeouts].time && t->heap[(idx << 1) + 1].time < t->heap[idx << 1].time) {
        t->heap[idx] = t->heap[(idx << 1) + 1];
        idx = (idx << 1) + 1;
      } else if(t->heap[idx << 1].time < t->heap[t->timeouts].time) {
        t->heap[idx] = t->heap[idx << 1];
        idx <<= 1;
      } else {
        break;
      }
    } while((idx << 1) <= t->timeouts);
    t->heap[idx] = t->heap[t->timeouts];
  }
}

int AddTimeout(struct Timeout* const timeout, const struct TimeoutObject* const work, const uint32_t amount) {
  int locked = pthread_mutex_trylock(&timeout->mutex);
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
  for(uint32_t i = 0; i < amount; ++i) {
    TimeoutHeapInsert(timeout, work[i]);
    (void) sem_post(&timeout->amount);
  }
  atomic_store(&timeout->latest, timeout->heap[1].time);
  if(locked != EBUSY) {
    (void) pthread_mutex_unlock(&timeout->mutex);
  }
  (void) sem_post(&timeout->work);
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

static void TimeoutThreadHandler(int sig, siginfo_t* info, void* ucontext) {
  if(timeout->onstop != NULL) {
    timeout->onstop(timeout);
  }
  if(pthread_mutex_destroy(&timeout->mutex) == EBUSY) {
    (void) pthread_mutex_unlock(&timeout->mutex);
    (void) pthread_mutex_destroy(&timeout->mutex);
  }
  (void) sem_destroy(&timeout->work);
  (void) sem_destroy(&timeout->amount);
  if(timeout->clear_mode == TIME_ALWAYS || (timeout->clear_mode == TIME_DEPENDS && atomic_load(&timeout->clean_work) == TIME_ALWAYS)) {
    timeout->timeouts = 1;
    timeout->max_timeouts = 1;
    free(timeout->heap);
    timeout->heap = NULL;
  }
  pthread_exit(NULL);
}

#undef timeout
#define timeout ((struct Timeout*) t)

static void* TimeoutThread(void* t) {
  uint64_t time;
  sigset_t mask;
  (void) sigfillset(&mask);
  (void) pthread_sigmask(SIG_BLOCK, &mask, NULL);
  (void) sigemptyset(&mask);
  (void) sigaddset(&mask, SIGUSR1);
  (void) pthread_sigmask(SIG_UNBLOCK, &mask, NULL);
  (void) sigaction(SIGUSR1, &((struct sigaction) {
    .sa_flags = SA_SIGINFO,
    .sa_sigaction = TimeoutThreadHandler
  }), NULL);
  if(timeout->onstart != NULL) {
    timeout->onstart(timeout);
  }
  while(1) {
    (void) sem_wait(&timeout->amount);
    while(1) {
      time = atomic_load(&timeout->latest);
      if(GetTime(0) >= time) {
        break;
      }
      (void) sem_timedwait(&timeout->work, &((struct timespec) { .tv_sec = time / 1000000000, .tv_nsec = time % 1000000000 }));
    }
    (void) pthread_mutex_lock(&timeout->mutex);
    TimeoutHeapPop(timeout);
    if(atomic_load(&timeout->clean_work) == TIME_ALWAYS) {
      timeout->heap = realloc(timeout->heap, sizeof(struct TimeoutObject) * timeout->timeouts);
      timeout->max_timeouts = timeout->timeouts;
    }
    (void) pthread_sigmask(SIG_BLOCK, &mask, NULL);
    timeout->heap[0].func(timeout->heap[0].data);
    (void) pthread_sigmask(SIG_UNBLOCK, &mask, NULL);
    (void) sigpending(&mask);
    if(sigismember(&mask, SIGUSR1)) {
      (void) pthread_mutex_unlock(&timeout->mutex);
      if(timeout->onstop != NULL) {
        timeout->onstop(timeout);
      }
      (void) pthread_mutex_destroy(&timeout->mutex);
      (void) sem_destroy(&timeout->work);
      (void) sem_destroy(&timeout->amount);
      if(timeout->clear_mode == TIME_ALWAYS || (timeout->clear_mode == TIME_DEPENDS && atomic_load(&timeout->clean_work) == TIME_ALWAYS)) {
        timeout->timeouts = 1;
        timeout->max_timeouts = 1;
        free(timeout->heap);
        timeout->heap = NULL;
      }
      pthread_exit(NULL);
    }
    atomic_store(&timeout->latest, timeout->heap[1].time);
    (void) pthread_mutex_unlock(&timeout->mutex);
  }
  return NULL;
}

#undef timeout

void StopTimeoutThread(struct Timeout* const timeout, const uint32_t clear_mode) {
  atomic_store(&timeout->clean_work, atomic_load(&timeout->clean_work) | 1);
  atomic_store(&timeout->clear_mode, clear_mode);
  (void) pthread_sigqueue(timeout->worker, SIGUSR1, (union sigval) { .sival_ptr = timeout });
}

int StartTimeoutThread(struct Timeout* const timeout, const uint32_t clean_work) {
  atomic_store(&timeout->clean_work, clean_work);
  int err = pthread_mutex_init(&timeout->mutex, NULL);
  if(err != 0) {
    return err;
  }
  err = sem_init(&timeout->work, 0, 0);
  if(err != 0) {
    (void) pthread_mutex_destroy(&timeout->mutex);
    return errno;
  }
  err = sem_init(&timeout->amount, 0, 0);
  if(err != 0) {
    (void) pthread_mutex_destroy(&timeout->mutex);
    (void) sem_destroy(&timeout->work);
    return errno;
  }
  err = pthread_create(&timeout->worker, NULL, TimeoutThread, timeout);
  if(err != 0) {
    (void) pthread_mutex_destroy(&timeout->mutex);
    (void) sem_destroy(&timeout->work);
    (void) sem_destroy(&timeout->amount);
  }
  return err;
}