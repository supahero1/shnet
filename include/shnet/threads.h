#ifndef _Px_6_UB_O_c7dZDKE3_I_hzvZK85iC_
#define _Px_6_UB_O_c7dZDKE3_I_hzvZK85iC_ 1

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <pthread.h>
#include <semaphore.h>

extern void pthread_cancel_on(void);

extern void pthread_cancel_off(void);

extern void pthread_async_on(void);

extern void pthread_async_off(void);

extern int  pthread_start(pthread_t* const, void* (*)(void*), void* const);

extern int  pthread_start_explicit(pthread_t* const, const pthread_attr_t* const, void* (*)(void*), void* const);

extern void pthread_cancel_sync(const pthread_t);

extern void pthread_cancel_async(const pthread_t);


typedef struct {
  pthread_t* ids;
  uint32_t used;
  uint32_t size;
} pthreads_t;

struct pthreads_data {
  void* (*func)(void*);
  void* arg;
  sem_t sem;
  pthread_mutex_t mutex;
#ifndef __cplusplus
  _Atomic
#endif
  uint32_t count;
};

extern int  pthreads_resize(pthreads_t* const, const uint32_t);

extern int  pthreads_start(pthreads_t* const, void* (*)(void*), void* const, const uint32_t);

extern int  pthreads_start_explicit(pthreads_t* const, const pthread_attr_t* const, void* (*)(void*), void* const, const uint32_t);

extern void pthreads_cancel(pthreads_t* const, const uint32_t);

extern void pthreads_cancel_sync(pthreads_t* const, const uint32_t);

extern void pthreads_cancel_async(pthreads_t* const, const uint32_t);

extern void pthreads_shutdown(pthreads_t* const);

extern void pthreads_shutdown_sync(pthreads_t* const);

extern void pthreads_shutdown_async(pthreads_t* const);

extern void pthreads_free(pthreads_t* const);


struct thread_pool_job {
  void (*func)(void*);
  void* data;
};

struct thread_pool {
  sem_t sem;
  uint32_t used;
  uint32_t size;
  pthread_mutex_t mutex;
  struct thread_pool_job* queue;
};

extern void* thread_pool_thread(void*);

extern int   thread_pool(struct thread_pool* const);

extern void  thread_pool_lock(struct thread_pool* const);

extern void  thread_pool_unlock(struct thread_pool* const);

extern int   thread_pool_resize_raw(struct thread_pool* const, const uint32_t);

extern int   thread_pool_resize(struct thread_pool* const, const uint32_t);

extern int   thread_pool_add_raw(struct thread_pool* const, void (*)(void*), void* const);

extern int   thread_pool_add(struct thread_pool* const, void (*)(void*), void* const);

extern void  thread_pool_try_work_raw(struct thread_pool* const);

extern void  thread_pool_try_work(struct thread_pool* const);

extern void  thread_pool_work_raw(struct thread_pool* const);

extern void  thread_pool_work(struct thread_pool* const);

extern void  thread_pool_clear_raw(struct thread_pool* const);

extern void  thread_pool_clear(struct thread_pool* const);

extern void  thread_pool_free(struct thread_pool* const);

#ifdef __cplusplus
}
#endif

#endif // _Px_6_UB_O_c7dZDKE3_I_hzvZK85iC_
