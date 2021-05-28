#ifndef __11_1Yvki_LNXnG7i_t6C_IE_7_ZZ1Z
#define __11_1Yvki_LNXnG7i_t6C_IE_7_ZZ1Z 1

#include "refheap.h"
#include "threads.h"

#include <stdint.h>
#include <pthread.h>
#include <stdatomic.h>
#include <semaphore.h>

enum time_consts {
  time_success,
  time_out_of_memory,
  time_failure,
  
  time_instant = 0,
  time_not_instant = 1
};

extern uint64_t time_sec_to_ns(const uint64_t);

extern uint64_t time_ms_to_ns(const uint64_t);

extern uint64_t time_us_to_ns(const uint64_t);

extern uint64_t time_get_ns(const uint64_t);

extern uint64_t time_get_us(const uint64_t);

extern uint64_t time_get_ms(const uint64_t);

extern uint64_t time_get_sec(const uint64_t);

extern uint64_t time_get_time(void);

struct time_timeout {
  uint64_t time;
  void (*func)(void*);
  void* data;
};

struct time_interval {
  uint64_t base_time;
  uint64_t interval;
  uint64_t count;
  void (*func)(void*);
  void* data;
};

struct time_manager {
  struct heap timeouts;
  struct heap intervals;
  struct threads thread;
  void (*on_timeout_expire)(struct time_manager*, const struct time_timeout*);
  void (*on_interval_expire)(struct time_manager*, const struct time_interval*);
  _Atomic uint64_t latest;
  pthread_mutex_t mutex;
  sem_t work;
  sem_t amount;
};

extern int time_manager(struct time_manager* const, void (*)(struct time_manager*, const struct time_timeout*), void (*)(struct time_manager*, const struct time_interval*), const unsigned long, const unsigned long);

extern int time_manager_start(struct time_manager* const);

extern int time_manager_add_timeout(struct time_manager* const, const uint64_t, void (*)(void*), void* const, struct time_timeout** const);

extern int time_manager_add_interval(struct time_manager* const, const uint64_t, const uint64_t, void (*)(void*), void* const, struct time_interval** const, const unsigned long);

extern void time_manager_cancel_timeout(struct time_manager* const, struct time_timeout* const);

extern void time_manager_cancel_interval(struct time_manager* const, struct time_interval* const);

extern void time_manager_stop(struct time_manager* const);

extern void time_manager_free(struct time_manager* const);

#endif // __11_1Yvki_LNXnG7i_t6C_IE_7_ZZ1Z