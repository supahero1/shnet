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

#include "distr.h"

#include <time.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <stdbool.h>

struct WorkerCluster WorkerCluster(const uint8_t clean_work) {
  return (struct WorkerCluster) {
    .work_amount = 0,
    .max_work_amount = 0,
    .clean_work = clean_work,
    .state = DISTR_DEAD,
    .lock = 0
  };
}

int Work(struct WorkerCluster* const cluster, void** const work, const uint32_t amount) {
  const uint8_t count = atomic_load(&cluster->count);
  uint8_t a, i = 0;
  void** ptr;
  (void) pthread_mutex_lock(&cluster->mutex);
  if(atomic_load(&cluster->clean_work) != DISTR_NEVER || cluster->max_work_amount - cluster->work_amount < amount) {
    ptr = realloc(cluster->work, sizeof(void*) * (cluster->work_amount + amount));
    if(ptr == NULL) {
      (void) pthread_mutex_unlock(&cluster->mutex);
      return ENOMEM;
    }
    cluster->work = ptr;
    cluster->max_work_amount = cluster->work_amount + amount;
  }
  (void) memmove(cluster->work + cluster->work_amount, work, sizeof(void*) * amount);
  if(cluster->work_amount == 0) {
    if((amount >> 1) >= count) {
      a = count;
    } else {
      a = amount >> 1;
    }
    for(; a > 0 && i < count; ++i) {
      if(atomic_load(&cluster->workers[i].state) == DISTR_SLEEPING) {
        (void) pthread_sigqueue(cluster->workers[i].thread, SIGRTMAX, (union sigval) { .sival_ptr = NULL });
        --a;
      }
    }
  }
  cluster->work_amount += amount;
  (void) pthread_mutex_unlock(&cluster->mutex);
  return 0;
}

void WorkCleanup(struct WorkerCluster* const cluster) {
  (void) pthread_mutex_lock(&cluster->mutex);
  if(cluster->work_amount != cluster->max_work_amount) {
    cluster->work = realloc(cluster->work, sizeof(void*) * cluster->work_amount);
    cluster->max_work_amount = cluster->work_amount;
  }
  (void) pthread_mutex_unlock(&cluster->mutex);
}

#define cluster ((struct Worker*) c)->c
#define id ((struct Worker*) c)->i

__nonnull((1)) static
void ClusterWorkerCleanup(void* c) {
  atomic_store(&cluster->workers[id].state, DISTR_DEAD);
  if(atomic_fetch_sub(&cluster->count, 1) == 1) {
    (void) pthread_mutex_destroy(&cluster->mutex);
    if(cluster->onclear != NULL) {
      cluster->onclear(cluster);
    }
    if((cluster->clear_mode == DISTR_DEPENDS && atomic_load(&cluster->clean_work) == DISTR_ALWAYS) || cluster->clear_mode == DISTR_ALWAYS) {
      cluster->work_amount = 0;
      cluster->max_work_amount = 0;
      free(cluster->work);
      cluster->work = NULL;
      free(cluster->workers);
      cluster->workers = NULL;
    }
    atomic_store(&cluster->state, DISTR_DEAD);
  }
}

static void dummy_signal_handler(__unused int sig) {}

__nonnull((1))
static void* ClusterWorker(void* c) {
  void* func;
  void* data;
  sigset_t mask;
  sigset_t rt_mask;
  uint8_t i = 0;
  if(atomic_load(&cluster->lock) == 1) {
    return NULL;
  }
  (void) sigfillset(&mask);
  (void) sigfillset(&rt_mask);
  (void) sigdelset(&rt_mask, SIGRTMAX);
  (void) pthread_sigmask(SIG_BLOCK, &mask, NULL);
  atomic_store(&cluster->workers[id].state, DISTR_SLEEPING);
  (void) sigaction(SIGRTMAX, &((struct sigaction) {
    .sa_flags = 0,
    .sa_handler = dummy_signal_handler
  }), NULL);
  pthread_cleanup_push(ClusterWorkerCleanup, c);
  for(; i < atomic_load(&cluster->count); ++i) {
    if(atomic_load(&cluster->workers[i].state) == DISTR_DEAD) {
      goto out;
    }
  }
  atomic_store(&cluster->state, DISTR_RUNNING);
  if(cluster->onready != NULL) {
    cluster->onready(cluster);
  }
  out:
  (void) sigsuspend(&rt_mask);
  (void) pthread_sigmask(SIG_UNBLOCK, &mask, NULL);
  atomic_store(&cluster->workers[id].state, DISTR_RUNNING);
  for(;;) {
    pthread_testcancel();
    (void) pthread_mutex_lock(&cluster->mutex);
    if(cluster->work_amount != 0) {
      func = cluster->work[0];
      data = cluster->work[1];
      cluster->work_amount -= 2;
      (void) memmove(cluster->work, cluster->work + 2, sizeof(void*) * cluster->work_amount);
      if(atomic_load(&cluster->clean_work) == DISTR_ALWAYS) {
        cluster->work = realloc(cluster->work, sizeof(void*) * cluster->work_amount);
        cluster->max_work_amount = cluster->work_amount;
      }
      (void) pthread_mutex_unlock(&cluster->mutex);
      ((void (*)(void*)) func)(data);
    } else {
      (void) pthread_mutex_unlock(&cluster->mutex);
      (void) pthread_sigmask(SIG_BLOCK, &mask, NULL);
      atomic_store(&cluster->workers[id].state, DISTR_SLEEPING);
      (void) sigsuspend(&rt_mask);
      (void) pthread_sigmask(SIG_UNBLOCK, &mask, NULL);
      atomic_store(&cluster->workers[id].state, DISTR_RUNNING);
    }
  }
  __unreachable;
  pthread_cleanup_pop(1);
  return NULL;
}

#undef cluster
#undef id

void ClearCluster(struct WorkerCluster* const cluster, const uint8_t clear_mode) {
  const uint8_t count = atomic_load(&cluster->count);
  uint8_t i = 0;
  atomic_store(&cluster->lock, 1);
  cluster->clear_mode = clear_mode;
  for(; i < count; ++i) {
    if(atomic_load(&cluster->workers[i].state) != DISTR_DEAD) {
      (void) pthread_cancel(cluster->workers[i].thread);
    }
    if(atomic_load(&cluster->workers[i].state) == DISTR_SLEEPING) {
      (void) pthread_sigqueue(cluster->workers[i].thread, SIGRTMAX, (union sigval) { .sival_ptr = NULL });
    }
  }
}

int PopulateCluster(struct WorkerCluster* const cluster, const uint8_t cores) {
  uint8_t i = 0;
  int err;
  atomic_store(&cluster->count, cores);
  atomic_store(&cluster->lock, 0);
  err = pthread_mutex_init(&cluster->mutex, NULL);
  if(err != 0) {
    return ENOMEM;
  }
  cluster->workers = malloc(sizeof(struct Worker) * cores);
  if(cluster->workers == NULL) {
    return ENOMEM;
  }
  for(; i < cores; ++i) {
    cluster->workers[i] = (struct Worker) {
      .c = cluster,
      .state = DISTR_DEAD,
      .i = i
    };
    err = pthread_create(&cluster->workers[i].thread, NULL, ClusterWorker, &cluster->workers[i]);
    if(err != 0) {
      atomic_store(&cluster->count, i);
      ClearCluster(cluster, DISTR_ALWAYS);
      return err;
    }
  }
  return 0;
}

void SetClusterWorkerReady(struct WorkerCluster* const cluster, const uint8_t which) {
  if(atomic_load(&cluster->workers[which].state) == DISTR_SLEEPING) {
    (void) pthread_sigqueue(cluster->workers[which].thread, SIGRTMAX, (union sigval) { .sival_ptr = NULL });
  }
}

void SetClusterWorkersReady(struct WorkerCluster* const cluster) {
  const uint8_t count = atomic_load(&cluster->count);
  uint8_t i = 0;
  for(; i < count; ++i) {
    if(atomic_load(&cluster->workers[i].state) == DISTR_SLEEPING) {
      (void) pthread_sigqueue(cluster->workers[i].thread, SIGRTMAX, (union sigval) { .sival_ptr = NULL });
    }
  }
}

struct TIStorage TIStorage(const uint8_t clean_work) {
  return (struct TIStorage) {
    .heap = NULL,
    .work_amount = 1,
    .max_work_amount = 1,
    .state = DISTR_DEAD,
    .clean_work = clean_work
  };
}

__nonnull((1))
static void TIHeap_insert(struct TIStorage* const storage, const struct TIObject obj) {
  uint32_t idx = storage->work_amount++;
  uint32_t parent = idx >> 1;
  if(idx == 1) {
    storage->heap[1] = obj;
  } else {
    while(storage->heap[parent].time > obj.time) {
      storage->heap[idx] = storage->heap[parent];
      if(parent == 1) {
        storage->heap[1] = obj;
        return;
      } else {
        idx = parent;
        parent >>= 1;
      }
    }
    storage->heap[idx] = obj;
  }
}

__nonnull((1))
static struct TIObject TIHeap_pop(struct TIStorage* const storage) {
  const struct TIObject obj = storage->heap[1];
  uint32_t idx = 1;
  --storage->work_amount;
  if((idx << 1) <= storage->work_amount) {
    do {
      if((idx << 1) + 1 <= storage->work_amount && storage->heap[(idx << 1) + 1].time < storage->heap[storage->work_amount].time && storage->heap[(idx << 1) + 1].time < storage->heap[idx << 1].time) {
        storage->heap[idx] = storage->heap[(idx << 1) + 1];
        idx = (idx << 1) + 1;
      } else if(storage->heap[idx << 1].time < storage->heap[storage->work_amount].time) {
        storage->heap[idx] = storage->heap[idx << 1];
        idx <<= 1;
      } else {
        break;
      }
    } while((idx << 1) <= storage->work_amount);
    storage->heap[idx] = storage->heap[storage->work_amount];
  }
  return obj;
}

uint64_t GetTime(const uint64_t nanoseconds) {
  struct timespec tp = { .tv_sec = 0, .tv_nsec = 0 };
  (void) clock_gettime(CLOCK_MONOTONIC, &tp);
  return nanoseconds + tp.tv_sec * 1000000000 + tp.tv_nsec;
}

int AddTimeout(struct TIStorage* const storage, const struct TIObject* const work, const uint32_t amount) {
  struct TIObject* ptr;
  uint32_t i = 0;
  (void) pthread_mutex_lock(&storage->mutex);
  if(atomic_load(&storage->clean_work) != DISTR_NEVER || storage->max_work_amount - storage->work_amount < amount) {
    ptr = realloc(storage->heap, sizeof(struct TIObject) * (storage->work_amount + amount));
    if(ptr == NULL) {
      (void) pthread_mutex_unlock(&storage->mutex);
      return ENOMEM;
    }
    storage->heap = ptr;
    storage->max_work_amount = storage->work_amount + amount;
  }
  for(; i < amount; ++i) {
    TIHeap_insert(storage, work[i]);
  }
  if(atomic_load(&storage->state) == DISTR_SLEEPING) {
    (void) pthread_sigqueue(storage->worker, SIGRTMAX, (union sigval) { .sival_ptr = NULL });
  }
  (void) pthread_mutex_unlock(&storage->mutex);
  return 0;
}

void TICleanup(struct TIStorage* const storage) {
  (void) pthread_mutex_lock(&storage->mutex);
  if(storage->work_amount != storage->max_work_amount) {
    storage->heap = realloc(storage->heap, sizeof(struct TIObject) * storage->work_amount);
    storage->max_work_amount = storage->work_amount;
  }
  (void) pthread_mutex_unlock(&storage->mutex);
}

#define storage ((struct TIStorage*) ti)

__nonnull((1))
static void TimeoutWorkerCleanup(void* ti) {
  (void) pthread_mutex_destroy(&storage->mutex);
  if(storage->onclear != NULL) {
    storage->onclear(storage);
  }
  if((storage->clear_mode == DISTR_DEPENDS && atomic_load(&storage->clean_work) == DISTR_ALWAYS) || storage->clear_mode == DISTR_ALWAYS) {
    storage->work_amount = 1;
    storage->max_work_amount = 1;
    free(storage->heap);
    storage->heap = NULL;
  }
  atomic_store(&storage->state, DISTR_DEAD);
}

__nonnull((1))
static void* TimeoutWorker(void* ti) {
  struct timespec t;
  struct TIObject obj;
  void (*callback)(void*);
  void* data;
  sigset_t mask;
  sigset_t rt_mask;
  (void) sigfillset(&mask);
  (void) sigfillset(&rt_mask);
  (void) sigdelset(&rt_mask, SIGRTMAX);
  (void) sigaction(SIGRTMAX, &((struct sigaction) {
    .sa_flags = 0,
    .sa_handler = dummy_signal_handler
  }), NULL);
  (void) pthread_sigmask(SIG_UNBLOCK, &mask, NULL);
  atomic_store(&storage->state, DISTR_RUNNING);
  pthread_cleanup_push(TimeoutWorkerCleanup, ti);
  if(storage->onready != NULL) {
    storage->onready(storage);
  }
  for(;;) {
    pthread_testcancel();
    (void) pthread_mutex_lock(&storage->mutex);
    if(storage->work_amount != 1) {
      storage->heap[0] = TIHeap_pop(storage);
      callback = storage->heap[0].func;
      data = storage->heap[0].data;
      if(atomic_load(&storage->clean_work) == DISTR_ALWAYS) {
        storage->heap = realloc(storage->heap, sizeof(struct TIObject) * storage->work_amount);
        storage->max_work_amount = storage->work_amount;
      }
      (void) pthread_mutex_unlock(&storage->mutex);
      t = (struct timespec) { .tv_sec = storage->heap[0].time / 1000000000, .tv_nsec = storage->heap[0].time % 1000000000 };
      atomic_store(&storage->state, DISTR_SLEEPING);
      while(clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &t, NULL) != 0) {
        (void) pthread_mutex_lock(&storage->mutex);
        atomic_store(&storage->state, DISTR_RUNNING);
        if(storage->heap[1].time < storage->heap[0].time) {
          obj = TIHeap_pop(storage);
          TIHeap_insert(storage, storage->heap[0]);
          storage->heap[0] = obj;
          callback = storage->heap[0].func;
          data = storage->heap[0].data;
          if(atomic_load(&storage->clean_work) == DISTR_ALWAYS) {
            storage->heap = realloc(storage->heap, sizeof(struct TIObject) * storage->work_amount);
            storage->max_work_amount = storage->work_amount;
          }
          atomic_store(&storage->state, DISTR_SLEEPING);
          (void) pthread_mutex_unlock(&storage->mutex);
          t = (struct timespec) { .tv_sec = storage->heap[0].time / 1000000000, .tv_nsec = storage->heap[0].time % 1000000000 };
        } else {
          (void) pthread_mutex_unlock(&storage->mutex);
        }
      }
      atomic_store(&storage->state, DISTR_RUNNING);
      callback(data);
    } else {
      (void) pthread_mutex_unlock(&storage->mutex);
      (void) pthread_sigmask(SIG_BLOCK, &mask, NULL);
      atomic_store(&storage->state, DISTR_SLEEPING);
      (void) sigsuspend(&rt_mask);
      (void) pthread_sigmask(SIG_UNBLOCK, &mask, NULL);
      atomic_store(&storage->state, DISTR_RUNNING);
    }
  }
  __unreachable;
  pthread_cleanup_pop(1);
  return NULL;
}

#undef storage

void ClearStorage(struct TIStorage* const storage, const uint8_t clear_mode) {
  storage->clear_mode = clear_mode;
  (void) pthread_cancel(storage->worker);
  if(atomic_load(&storage->state) == DISTR_SLEEPING) {
    (void) pthread_sigqueue(storage->worker, SIGRTMAX, (union sigval) { .sival_ptr = NULL });
  }
}

int DeployTimeout(struct TIStorage* const storage) {
  int err = pthread_mutex_init(&storage->mutex, NULL);
  if(err != 0) {
    return ENOMEM;
  }
  return pthread_create(&storage->worker, NULL, TimeoutWorker, storage);
}