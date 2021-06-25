#ifndef _Px_6_UB_O_c7dZDKE3_I_hzvZK85iC_
#define _Px_6_UB_O_c7dZDKE3_I_hzvZK85iC_ 1

#include <pthread.h>
#include <stdatomic.h>
#include <semaphore.h>

enum threads_consts {
  threads_success,
  threads_out_of_memory,
  threads_failure
};

struct threads {
  pthread_t* threads;
  unsigned used;
  unsigned size;
  _Atomic unsigned togo;
  pthread_barrier_t* barrier;
  sem_t sem;
  void (*func)(void*);
  void* data;
};

extern int threads(struct threads* const);

extern int threads_resize(struct threads* const, const unsigned);

extern int threads_add(struct threads* const, const unsigned);

extern void threads_remove(struct threads* const, const unsigned);

extern void threads_shutdown(struct threads* const);

extern void threads_free(struct threads* const);

#endif // _Px_6_UB_O_c7dZDKE3_I_hzvZK85iC_