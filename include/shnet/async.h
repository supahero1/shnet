#ifndef xuspO3U__sJ_6UGWy_hM1A_r_zOz_zX_
#define xuspO3U__sJ_6UGWy_hM1A_r_zOz_zX_ 1

#include <stdint.h>
#include <sys/epoll.h>

#include <shnet/threads.h>

struct async_event {
  int fd;
  uint8_t socket:1;
  uint8_t server:1;
  uint8_t wakeup:1;
  uint8_t secure:1;
  _Atomic uint16_t flags;
};

struct async_loop {
  struct async_event** fake_events;
  struct epoll_event* events;
  void (*on_event)(struct async_loop*, uint32_t, struct async_event*);
  
  pthread_t thread;
  pthread_mutex_t lock;
  struct async_event evt;
  
  uint32_t fake_events_len;
  int events_len;
  int fd;
};

extern void* async_loop_thread(void*);

extern int   async_loop(struct async_loop* const);

extern int   async_loop_start(struct async_loop* const);

extern void  async_loop_stop(struct async_loop* const);

extern void  async_loop_free(struct async_loop* const);

extern void  async_loop_reset(struct async_loop* const);

extern void  async_loop_push_joinable(struct async_loop* const);

extern void  async_loop_push_free(struct async_loop* const);

extern void  async_loop_push_ptr_free(struct async_loop* const);

extern void  async_loop_commit(struct async_loop* const);

extern int   async_loop_add(const struct async_loop* const, struct async_event* const, const uint32_t);

extern int   async_loop_mod(const struct async_loop* const, struct async_event* const, const uint32_t);

extern int   async_loop_remove(const struct async_loop* const, struct async_event* const);

extern int   async_loop_create_event(struct async_loop* const, struct async_event* const);

extern int   async_loop_create_events(struct async_loop* const, struct async_event* const, const uint32_t);

#endif // xuspO3U__sJ_6UGWy_hM1A_r_zOz_zX_