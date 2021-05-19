#ifndef __11_1Yvki_LNXnG7i_t6C_IE_7_ZZ1Z
#define __11_1Yvki_LNXnG7i_t6C_IE_7_ZZ1Z 1

#include "avl.h"
#include "heap.h"
#include "misc.h"
#include "threads.h"

#include <stdint.h>
#include <pthread.h>
#include <stdatomic.h>
#include <semaphore.h>

enum time_consts {
  time_success,
  time_out_of_memory,
  time_failure,
  
  time_interval = 1
};

extern uint64_t time_get_ns(const uint64_t);

extern uint64_t time_get_us(const uint64_t);

extern uint64_t time_get_ms(const uint64_t);

extern uint64_t time_get_sec(const uint64_t);

struct time_manager_node {
  uint64_t time;
  uint32_t id;
  uint32_t interval;
  void (*func)(void*);
  void* data;
};

struct time_manager_tree_node {
  struct avl_node tree_node;
  struct time_manager_node node;
};

struct time_manager {
  struct avl_tree tree;
  struct contmem contmem;
  struct threads thread;
  unsigned long counter;
  void (*on_timer_expire)(struct time_manager*, void*);
  _Atomic uint64_t latest;
  uint64_t* latest_ptr;
  pthread_mutex_t mutex;
  sem_t work;
  sem_t amount;
};

extern int time_manager(struct time_manager* const, void (*)(struct time_manager*, void*), const unsigned long, const unsigned long);

extern int time_manager_start(struct time_manager* const);

extern void time_manager_cancel_timer(struct time_manager* const, const uint64_t, const uint32_t);

extern uint32_t time_manager_add_timer(struct time_manager* const, const uint64_t, void (*)(void*), void* const, const int);

extern void time_manager_stop(struct time_manager* const);

extern void time_manager_free(struct time_manager* const);

#endif // __11_1Yvki_LNXnG7i_t6C_IE_7_ZZ1Z