#ifndef xuspO3U__sJ_6UGWy_hM1A_r_zOz_zX_
#define xuspO3U__sJ_6UGWy_hM1A_r_zOz_zX_ 1

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <sys/epoll.h>

#include <shnet/threads.h>

typedef uint16_t async_flag_raw_t;
#ifdef __cplusplus
typedef async_flag_raw_t async_flag_t;
#else
typedef _Atomic async_flag_raw_t async_flag_t;
#endif // __cplusplus

struct async_event {
  int fd;
  uint8_t socket:1;
  uint8_t server:1;
  uint8_t wakeup:1;
  async_flag_t flags;
};

struct async_loop {
  struct epoll_event* events;
  void (*on_event)(struct async_loop*, uint32_t, struct async_event*);
  
  pthread_t thread;
  struct async_event evt;
  
  int events_len;
  int fd;
};

extern void* async_loop_thread(void*);

extern int   async_loop(struct async_loop* const);

extern int   async_loop_start(struct async_loop* const);

extern void  async_loop_stop(struct async_loop* const);

extern void  async_loop_free(struct async_loop* const);

extern void  async_loop_push_joinable(struct async_loop* const);

extern void  async_loop_push_free(struct async_loop* const);

extern void  async_loop_push_ptr_free(struct async_loop* const);

extern void  async_loop_commit(struct async_loop* const);

extern int   async_loop_add(const struct async_loop* const, struct async_event* const, const uint32_t);

extern int   async_loop_mod(const struct async_loop* const, struct async_event* const, const uint32_t);

extern int   async_loop_remove(const struct async_loop* const, struct async_event* const);

#ifdef __cplusplus
}
#endif

#endif // xuspO3U__sJ_6UGWy_hM1A_r_zOz_zX_
