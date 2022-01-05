#ifndef _Px_6_UB_O_c7dZDKE3_I_hzvZK85iC_
#define _Px_6_UB_O_c7dZDKE3_I_hzvZK85iC_ 1

#include <stdint.h>
#include <pthread.h>
#include <stdatomic.h>
#include <semaphore.h>

extern void thread_cancellation_disable(void);

extern void thread_cancellation_enable(void);

extern void thread_cancellation_async(void);

extern void thread_cancellation_deferred(void);

struct thread {
  pthread_t thread;
};

struct thread_data {
  void* data;
  void (*func)(void*);
};

extern int  thread_start(struct thread* const, void (*)(void*), void* const);

extern void thread_stop(struct thread* const);

extern void thread_stop_async(struct thread* const);


struct threads {
  uint32_t used;
  uint32_t size;
  pthread_t* threads;
};

struct threads_data {
  void* data;
  void (*func)(void*);
  pthread_mutex_t* mutex;
  pthread_barrier_t barrier;
  _Atomic uint32_t count;
};

extern int  threads_resize(struct threads* const, const uint32_t);

extern int  threads_add(struct threads* const, void (*)(void*), void* const, const uint32_t);

extern void threads_remove(struct threads* const, const uint32_t);

extern void threads_remove_async(struct threads* const, const uint32_t);

extern void threads_shutdown(struct threads* const);

extern void threads_shutdown_async(struct threads* const);

extern void threads_free(struct threads* const);


struct thread_pool {
  sem_t sem;
  uint32_t used;
  uint32_t size;
  pthread_mutex_t mutex;
  struct thread_data* queue;
};

extern void thread_pool_thread(void*);

extern int  thread_pool(struct thread_pool* const);

extern void thread_pool_lock(struct thread_pool* const);

extern void thread_pool_unlock(struct thread_pool* const);

extern int  thread_pool_resize_raw(struct thread_pool* const, const uint32_t);

extern int  thread_pool_resize(struct thread_pool* const, const uint32_t);

extern int  thread_pool_add_raw(struct thread_pool* const, void (*)(void*), void* const);

extern int  thread_pool_add(struct thread_pool* const, void (*)(void*), void* const);

extern void thread_pool_try_work_raw(struct thread_pool* const);

extern void thread_pool_try_work(struct thread_pool* const);

extern void thread_pool_work_raw(struct thread_pool* const);

extern void thread_pool_work(struct thread_pool* const);

extern void thread_pool_clear_raw(struct thread_pool* const);

extern void thread_pool_clear(struct thread_pool* const);

extern void thread_pool_free(struct thread_pool* const);

#endif // _Px_6_UB_O_c7dZDKE3_I_hzvZK85iC_