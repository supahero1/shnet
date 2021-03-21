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

#include <stdint.h>
#include <pthread.h>
#include <stdatomic.h>

#define TIME_DEAD     0
#define TIME_ALIVE    1

#define TIME_NEVER    0
#define TIME_DEPENDS  1
#define TIME_ALWAYS   2

extern uint64_t GetTime(const uint64_t);

struct TimeoutObject {
  uint64_t time;
  void (*func)(void*);
  void* data;
};

struct Timeout {
  void (*onready)(struct Timeout*);
  void (*onclear)(struct Timeout*);
  pthread_t worker;
  struct TimeoutObject* heap;
  pthread_mutex_t mutex;
  uint32_t timeouts;
  uint32_t max_timeouts;
  _Atomic uint32_t state;
  uint32_t clear_mode;
  _Atomic uint32_t clean_work;
};

__const
extern struct Timeout Timeout(void);

__nonnull((1, 2))
extern int AddTimeout(struct Timeout* const, const struct TimeoutObject* const, const uint32_t);

__nonnull((1))
extern void TimeoutCleanup(struct Timeout* const);

__nonnull((1))
extern void StopTimeoutThread(struct Timeout* const, const uint32_t);

extern int StartTimeoutThread(struct Timeout* const, const uint32_t);

#ifdef __cplusplus
}
#endif

#endif // f_CPk_VQMzsBbdk_fxJrua_7Af_ItkDT