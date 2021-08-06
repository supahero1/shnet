#ifndef _Px_6_UB_O_c7dZDKE3_I_hzvZK85iC_
#define _Px_6_UB_O_c7dZDKE3_I_hzvZK85iC_ 1

#include <stdint.h>
#include <pthread.h>
#include <stdatomic.h>
#include <semaphore.h>

struct threads {
  pthread_t* threads;
  pthread_barrier_t barrier;
  void (*func)(void*);
  void* data;
  sem_t sem;
  uint32_t used;
  uint32_t size;
  _Atomic uint32_t togo;
};

extern int  threads(struct threads* const);

extern int  threads_resize(struct threads* const, const uint32_t);

extern int  threads_add(struct threads* const, const uint32_t);

extern void threads_remove(struct threads* const, const uint32_t);

extern void threads_shutdown(struct threads* const);

extern void threads_free(struct threads* const);


struct thread {
  pthread_t thread;
};

extern int  thread_start(struct thread* const, void* (*)(void*), void* const);

extern void thread_stop(struct thread* const);

#endif // _Px_6_UB_O_c7dZDKE3_I_hzvZK85iC_