#ifndef __11_1Yvki_LNXnG7i_t6C_IE_7_ZZ1Z
#define __11_1Yvki_LNXnG7i_t6C_IE_7_ZZ1Z 1

#include "refheap.h"
#include "threads.h"

#include <stdint.h>
#include <pthread.h>
#include <stdatomic.h>
#include <semaphore.h>

extern uint64_t time_sec_to_ms(const uint64_t);

extern uint64_t time_sec_to_us(const uint64_t);

extern uint64_t time_sec_to_ns(const uint64_t);


extern uint64_t time_ms_to_sec(const uint64_t);

extern uint64_t time_ms_to_us(const uint64_t);

extern uint64_t time_ms_to_ns(const uint64_t);


extern uint64_t time_us_to_ns(const uint64_t);

extern uint64_t time_us_to_sec(const uint64_t);

extern uint64_t time_us_to_ms(const uint64_t);


extern uint64_t time_ns_to_sec(const uint64_t);

extern uint64_t time_ns_to_ms(const uint64_t);

extern uint64_t time_ns_to_us(const uint64_t);


extern uint64_t time_get_sec(const uint64_t);

extern uint64_t time_get_ms(const uint64_t);

extern uint64_t time_get_us(const uint64_t);

extern uint64_t time_get_ns(const uint64_t);

extern uint64_t time_get_time(void);


struct time_reference {
  uint64_t timer;
  uint64_t last_time;
};

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
  pthread_mutex_t mutex;
  _Atomic uint64_t latest;
  sem_t work;
  sem_t amount;
  struct thread thread;
};

extern int  time_manager(struct time_manager* const);

extern int  time_manager_start(struct time_manager* const);

extern void time_manager_lock(struct time_manager* const);

extern void time_manager_unlock(struct time_manager* const);

extern void time_manager_stop(struct time_manager* const);

extern void time_manager_stop_async(struct time_manager* const);

extern void time_manager_free(struct time_manager* const);


extern int  time_manager_resize_timeouts_raw(struct time_manager* const, const uint64_t);

extern int  time_manager_resize_timeouts(struct time_manager* const, const uint64_t);

extern int  time_manager_add_timeout_raw(struct time_manager* const, const uint64_t, void (*)(void*), void* const, struct time_reference* const);

extern int  time_manager_add_timeout(struct time_manager* const, const uint64_t, void (*)(void*), void* const, struct time_reference* const);

extern int  time_manager_cancel_timeout_raw(struct time_manager* const, struct time_reference* const);

extern int  time_manager_cancel_timeout(struct time_manager* const, struct time_reference* const);

extern struct time_timeout* time_manager_open_timeout_raw(struct time_manager* const, struct time_reference* const);

extern struct time_timeout* time_manager_open_timeout(struct time_manager* const, struct time_reference* const);

extern void time_manager_close_timeout_raw(struct time_manager* const, struct time_reference* const);

extern void time_manager_close_timeout(struct time_manager* const, struct time_reference* const);


extern int  time_manager_resize_intervals_raw(struct time_manager* const, const uint64_t);

extern int  time_manager_resize_intervals(struct time_manager* const, const uint64_t);

extern int  time_manager_add_interval_raw(struct time_manager* const, const uint64_t, const uint64_t, void (*)(void*), void* const, struct time_reference* const);

extern int  time_manager_add_interval(struct time_manager* const, const uint64_t, const uint64_t, void (*)(void*), void* const, struct time_reference* const);

extern int  time_manager_cancel_interval_raw(struct time_manager* const, struct time_reference* const);

extern int  time_manager_cancel_interval(struct time_manager* const, struct time_reference* const);

extern struct time_interval* time_manager_open_interval_raw(struct time_manager* const, struct time_reference* const);

extern struct time_interval* time_manager_open_interval(struct time_manager* const, struct time_reference* const);

extern void time_manager_close_interval_raw(struct time_manager* const, struct time_reference* const);

extern void time_manager_close_interval(struct time_manager* const, struct time_reference* const);

#endif // __11_1Yvki_LNXnG7i_t6C_IE_7_ZZ1Z