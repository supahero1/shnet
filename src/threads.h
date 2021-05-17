#ifndef _Px_6_UB_O_c7dZDKE3_I_hzvZK85iC_
#define _Px_6_UB_O_c7dZDKE3_I_hzvZK85iC_ 1

#include <pthread.h>
#include <stdatomic.h>
#include <semaphore.h>

enum threads_consts {
  threads_success,
  threads_out_of_memory,
  threads_failure,
  
  threads_creation_error = 0,
  threads_close,
  
  threads_detached = 0,
  threads_joinable
};

struct threads {
  pthread_t* threads;
  unsigned long size;
  _Atomic unsigned long used;
  _Atomic int flag;
  pthread_barrier_t* barrier;
  void (*startup)(void*);
  void* data;
  void (*on_start)(struct threads*);
  void (*on_stop)(struct threads*);
};

extern int threads_add(struct threads* const, const unsigned long, const int);

extern int threads_remove(struct threads* const, const unsigned long);

extern int threads_shutdown(struct threads* const);

#endif // _Px_6_UB_O_c7dZDKE3_I_hzvZK85iC_