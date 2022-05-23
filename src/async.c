#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include <stdatomic.h>
#include <sys/eventfd.h>

#include <shnet/async.h>
#include <shnet/error.h>

#define loop ((struct async_loop*) async_loop_thread_data)
#define event ((struct async_event*) loop->events[i].data.ptr)

void* async_loop_thread(void* async_loop_thread_data) {
  pthread_cancel_off();
  while(1) {
    const int count = epoll_wait(loop->fd, loop->events, loop->events_len, -1);
    assert(count >= 0);
    for(int i = 0; i < count; ++i) {
      if(event->wakeup) {
        const async_flag_raw_t flags = atomic_load_explicit(&loop->evt.flags, memory_order_acquire);
        atomic_store_explicit(&loop->evt.flags, 0, memory_order_release);
        if(!(flags & 1)) {
          (void) pthread_detach(loop->thread);
        }
        if(flags & 2) {
          async_loop_free(loop);
        } else {
          eventfd_t temp;
          assert(!eventfd_read(loop->evt.fd, &temp));
          (void) temp;
        }
        if(flags & 4) {
          free(loop);
        }
        return NULL;
      } else {
        loop->on_event(loop, loop->events[i].events, event);
      }
    }
  }
  assert(0);
}

#undef event
#undef loop

int async_loop(struct async_loop* const loop) {
  if(loop->events_len == 0) {
    loop->events_len = 64;
  }
  loop->events = shnet_malloc(sizeof(*loop->events) * loop->events_len);
  if(loop->events == NULL) {
    return -1;
  }
  int fd;
  safe_execute(fd = epoll_create1(0), fd == -1, errno);
  if(fd == -1) {
    goto err_e;
  }
  loop->fd = fd;
  safe_execute(fd = eventfd(0, EFD_NONBLOCK), fd == -1, errno);
  if(fd == -1) {
    goto err_fd;
  }
  loop->evt.fd = fd;
  loop->evt.wakeup = 1;
  if(async_loop_add(loop, &loop->evt, EPOLLIN) == -1) {
    goto err_efd;
  }
  return 0;
  
  err_efd:
  (void) close(loop->evt.fd);
  err_fd:
  (void) close(loop->fd);
  err_e:
  free(loop->events);
  return -1;
}

int async_loop_start(struct async_loop* const loop) {
  atomic_store_explicit(&loop->evt.flags, 0, memory_order_release);
  return pthread_start(&loop->thread, async_loop_thread, loop);
}

void async_loop_stop(struct async_loop* const loop) {
  async_loop_push_joinable(loop);
  assert(!eventfd_write(loop->evt.fd, 1));
  (void) pthread_join(loop->thread, NULL);
}

void async_loop_free(struct async_loop* const loop) {
  (void) close(loop->fd);
  (void) close(loop->evt.fd);
  free(loop->events);
  loop->events = NULL;
}

void async_loop_push_joinable(struct async_loop* const loop) {
  atomic_fetch_or_explicit(&loop->evt.flags, 1, memory_order_acq_rel);
}

void async_loop_push_free(struct async_loop* const loop) {
  atomic_fetch_or_explicit(&loop->evt.flags, 2, memory_order_acq_rel);
}

void async_loop_push_ptr_free(struct async_loop* const loop) {
  atomic_fetch_or_explicit(&loop->evt.flags, 4, memory_order_acq_rel);
}

void async_loop_commit(struct async_loop* const loop) {
  assert(!eventfd_write(loop->evt.fd, 1));
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
