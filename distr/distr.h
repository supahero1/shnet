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

#ifndef f_CPk_VQMzsBbdk_fxJrua_7Af_ItkDT
#define f_CPk_VQMzsBbdk_fxJrua_7Af_ItkDT 1

#ifdef __cplusplus
extern "C" {
#endif

#include "../def.h"

#include <stdatomic.h>
#include <pthread.h>
#include <stdint.h>

#define DISTR_DEAD      0
#define DISTR_RUNNING   1
#define DISTR_SLEEPING  2

#define DISTR_NEVER     0
#define DISTR_DEPENDS   1
#define DISTR_ALWAYS    2

struct Worker {
  struct WorkerCluster* c;
  pthread_t thread;
  _Atomic uint8_t state;
  uint8_t i;
};

struct WorkerCluster {
  pthread_mutex_t mutex;
  struct Worker* workers;
  void (*onready)(struct WorkerCluster*);
  void (*onclear)(struct WorkerCluster*);
  void** work;
  uint32_t work_amount;
  uint32_t max_work_amount;
  uint8_t clear_mode;
  _Atomic uint8_t clean_work;
  _Atomic uint8_t state;
  _Atomic uint8_t lock;
  _Atomic uint8_t count;
};

__const
extern struct WorkerCluster WorkerCluster(const uint8_t);

__nonnull((1, 2))
extern int Work(struct WorkerCluster* const, void** const, const uint32_t);

__nonnull((1))
extern void WorkCleanup(struct WorkerCluster* const);

__nonnull((1))
extern void ClearCluster(struct WorkerCluster* const, const uint8_t);

__nonnull((1))
extern int PopulateCluster(struct WorkerCluster* const, const uint8_t);

__nonnull((1))
extern void SetClusterWorkerReady(struct WorkerCluster* const, const uint8_t);

__nonnull((1))
extern void SetClusterWorkersReady(struct WorkerCluster* const);

struct TIObject {
  uint64_t time;
  void (*func)(void*);
  void* data;
};

struct TIStorage {
  pthread_mutex_t mutex;
  void (*onready)(struct TIStorage*);
  void (*onclear)(struct TIStorage*);
  pthread_t worker;
  struct TIObject* heap;
  uint32_t work_amount;
  uint32_t max_work_amount;
  uint8_t state;
  uint8_t clear_mode;
  _Atomic uint8_t clean_work;
};

__const
extern struct TIStorage TIStorage(const uint8_t);

extern uint64_t GetTime(const uint64_t);

__nonnull((1, 2))
extern int AddTimeout(struct TIStorage* const, const struct TIObject* const, const uint32_t);

__nonnull((1))
extern void TICleanup(struct TIStorage* const);

__nonnull((1))
extern void ClearStorage(struct TIStorage* const, const uint8_t);

extern int DeployTimeout(struct TIStorage* const);

#ifdef __cplusplus
}
#endif

#endif // f_CPk_VQMzsBbdk_fxJrua_7Af_ItkDT