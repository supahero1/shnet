#ifndef __11_1Yvki_LNXnG7i_t6C_IE_7_ZZ1Z
#define __11_1Yvki_LNXnG7i_t6C_IE_7_ZZ1Z 1

#include <stdint.h>

#include <shnet/threads.h>

#define TIME_IMMEDIATELY 2

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


struct time_timer {
  uint32_t ref;
};

struct time_timeout {
  struct time_timer* ref;
  void (*func)(void*);
  uint64_t time;
  void* data;
};

struct time_interval {
  struct time_timer* ref;
  void (*func)(void*);
  uint64_t base_time;
  uint64_t interval;
  uint64_t count;
  void* data;
};

struct time_timers {
  struct time_timeout* timeouts;
  struct time_interval* intervals;
  
  _Atomic uint64_t latest;
  sem_t work;
  sem_t updates;
  pthread_mutex_t mutex;
  pthread_t thread;
  
  uint32_t timeouts_used;
  uint32_t timeouts_size;
  uint32_t intervals_used;
  uint32_t intervals_size;
};

extern int  time_timers(struct time_timers* const);

extern int  time_timers_start(struct time_timers* const);

extern void time_timers_stop(struct time_timers* const);

extern void time_timers_stop_async(struct time_timers* const);

extern void time_timers_stop_joinable(struct time_timers* const);

extern void time_timers_free(struct time_timers* const);

extern void time_lock(struct time_timers* const);

extern void time_unlock(struct time_timers* const);


extern int  time_resize_timeouts_raw(struct time_timers* const, const uint32_t);

extern int  time_resize_timeouts(struct time_timers* const, const uint32_t);

extern int  time_add_timeout_raw(struct time_timers* const, const struct time_timeout* const);

extern int  time_add_timeout(struct time_timers* const, const struct time_timeout* const);

extern int  time_cancel_timeout_raw(struct time_timers* const, struct time_timer* const);

extern int  time_cancel_timeout(struct time_timers* const, struct time_timer* const);

extern struct time_timeout* time_open_timeout_raw(struct time_timers* const, struct time_timer* const);

extern struct time_timeout* time_open_timeout(struct time_timers* const, struct time_timer* const);

extern void time_close_timeout_raw(struct time_timers* const, struct time_timer* const);

extern void time_close_timeout(struct time_timers* const, struct time_timer* const);


extern int  time_resize_intervals_raw(struct time_timers* const, const uint32_t);

extern int  time_resize_intervals(struct time_timers* const, const uint32_t);

extern int  time_add_interval_raw(struct time_timers* const, const struct time_interval* const);

extern int  time_add_interval(struct time_timers* const, const struct time_interval* const);

extern int  time_cancel_interval_raw(struct time_timers* const, struct time_timer* const);

extern int  time_cancel_interval(struct time_timers* const, struct time_timer* const);

extern struct time_interval* time_open_interval_raw(struct time_timers* const, struct time_timer* const);

extern struct time_interval* time_open_interval(struct time_timers* const, struct time_timer* const);

extern void time_close_interval_raw(struct time_timers* const, struct time_timer* const);

extern void time_close_interval(struct time_timers* const, struct time_timer* const);

#endif // __11_1Yvki_LNXnG7i_t6C_IE_7_ZZ1Z