#ifndef f_CPk_VQMzsBbdk_fxJrua_7Af_ItkDT
#define f_CPk_VQMzsBbdk_fxJrua_7Af_ItkDT 1

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <pthread.h>
#include <stdatomic.h>
#include <semaphore.h>

#define TIME_NEVER    2
#define TIME_DEPENDS  4
#define TIME_ALWAYS   6

extern uint64_t GetTime(const uint64_t);

struct TimeoutObject {
  uint64_t time;
  void (*func)(void*);
  void* data;
};

struct Timeout {
  void (*onstart)(struct Timeout*);
  void (*onstop)(struct Timeout*);
  pthread_t worker;
  struct TimeoutObject* heap;
  _Atomic uint64_t latest;
  pthread_mutex_t mutex;
  sem_t work;
  sem_t amount;
  uint32_t timeouts;
  uint32_t max_timeouts;
  _Atomic uint32_t clear_mode;
  _Atomic uint32_t clean_work;
};

extern struct Timeout Timeout(void);

extern int SetTimeout(struct Timeout* const, const struct TimeoutObject* const, const uint32_t);

extern void TimeoutCleanup(struct Timeout* const);

extern void StopTimeoutThread(struct Timeout* const, const uint32_t);

extern int StartTimeoutThread(struct Timeout* const, const uint32_t);

#ifdef __cplusplus
}
#endif

#endif // f_CPk_VQMzsBbdk_fxJrua_7Af_ItkDT