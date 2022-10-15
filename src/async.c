#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include <sys/eventfd.h>

#include <shnet/async.h>
#include <shnet/error.h>

#define loop ((struct async_loop*) async_loop_thread_data)
#define event ((struct async_event*) loop->events[i].data.ptr)
#define mask (loop->events[i].events)

void* async_loop_thread(void* async_loop_thread_data) {
  pthread_cancel_off();
  while(1) {
    const int count = epoll_wait(loop->fd, loop->events, loop->events_len, -1);
    for(int i = 0; i < count; ++i) {
      if(event->fd == loop->evt.fd) {
        assert(mask == EPOLLIN);
        eventfd_t flags;
        assert(!eventfd_read(loop->evt.fd, &flags));
        flags >>= 1;
        if(!(flags & async_joinable)) {
          (void) pthread_detach(loop->thread);
        } 
        if(flags & async_free) {
          async_loop_free(loop);
        }
        if(flags & async_ptr_free) {
          free(loop);
        }
        return NULL;
      } else {
        loop->on_event(loop, mask, event);
      }
    }
  }
  assert(0);
}

#undef mask
#undef event
#undef loop

int async_loop(struct async_loop* const loop) {
  if(loop->events_len == 0) {
    loop->events_len = 64;
  }
  if(loop->events == NULL) {
    loop->events = shnet_malloc(sizeof(*loop->events) * loop->events_len);
    if(loop->events == NULL) {
      return -1;
    }
    loop->alloc_events = 1;
  }
  safe_execute(loop->fd = epoll_create1(0), loop->fd == -1, errno);
  if(loop->fd == -1) {
    goto err_e;
  }
  safe_execute(loop->evt.fd = eventfd(0, EFD_NONBLOCK), loop->evt.fd == -1, errno);
  if(loop->evt.fd == -1) {
    goto err_fd;
  }
  if(async_loop_add(loop, &loop->evt, EPOLLIN) == -1) {
    goto err_efd;
  }
  return 0;
  
  err_efd:
  (void) close(loop->evt.fd);
  err_fd:
  (void) close(loop->fd);
  err_e:
  if(loop->alloc_events) {
    free(loop->events);
    loop->events = NULL;
    loop->alloc_events = 0;
  }
  return -1;
}

int async_loop_start(struct async_loop* const loop) {
  return pthread_start(&loop->thread, async_loop_thread, loop);
}

void async_loop_stop(const struct async_loop* const loop) {
  async_loop_shutdown(loop, async_joinable);
  (void) pthread_join(loop->thread, NULL);
}

void async_loop_free(struct async_loop* const loop) {
  (void) close(loop->fd);
  (void) close(loop->evt.fd);
  if(loop->alloc_events) {
    free(loop->events);
    loop->events = NULL;
    loop->alloc_events = 0;
  }
}

void async_loop_shutdown(const struct async_loop* const loop, const enum async_shutdown flags) {
  assert(!eventfd_write(loop->evt.fd, 1 | (flags << 1)));
}

static int async_loop_modify(const struct async_loop* const loop, struct async_event* const event, const int method, const uint32_t events) {
  int err;
  safe_execute(err = epoll_ctl(loop->fd, method, event->fd, method == EPOLL_CTL_DEL ? NULL : &((struct epoll_event) {
    .events = events,
    .data = (epoll_data_t) {
      .ptr = event
    }
  })), err == -1, errno);
  return err;
}

int async_loop_add(const struct async_loop* const loop, struct async_event* const event, const uint32_t events) {
  return async_loop_modify(loop, event, EPOLL_CTL_ADD, events);
}

int async_loop_mod(const struct async_loop* const loop, struct async_event* const event, const uint32_t events) {
  return async_loop_modify(loop, event, EPOLL_CTL_MOD, events);
}

int async_loop_remove(const struct async_loop* const loop, struct async_event* const event) {
  return async_loop_modify(loop, event, EPOLL_CTL_DEL, 0);
}
