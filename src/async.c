#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include <string.h>
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
    for(int i = 0; i < count; ++i) {
      if(event->wakeup) {
        atomic_thread_fence(memory_order_acquire);
        const uint8_t flags = atomic_load_explicit(&loop->evt.flags, memory_order_relaxed);
        if(flags != 0) {
          if(!(flags & 1)) {
            (void) pthread_detach(loop->thread);
          }
          if(flags & 2) {
            async_loop_free(loop);
          }
          if(flags & 4) {
            free(loop);
          }
          return NULL;
        }
        uint64_t r;
        (void) eventfd_read(loop->evt.fd, &r);
      } else {
        loop->on_event(loop, loop->events[i].events, event);
      }
    }
    (void) pthread_mutex_lock(&loop->lock);
    if(loop->fake_events != NULL) {
      for(uint32_t i = 0; i < loop->fake_events_len; ++i) {
        loop->on_event(loop, 0, loop->fake_events[i]);
      }
      free(loop->fake_events);
      loop->fake_events = NULL;
      loop->fake_events_len = 0;
    }
    (void) pthread_mutex_unlock(&loop->lock);
  }
  assert(0);
}

#undef event
#undef loop

int async_loop(struct async_loop* const loop) {
  if(loop->events_len == 0) {
    loop->events_len = 100;
  }
  safe_execute(loop->events = malloc(sizeof(*loop->events) * loop->events_len), loop->events == NULL, ENOMEM);
  if(loop->events == NULL) {
    return -1;
  }
  int fd;
  safe_execute(fd = epoll_create1(0), fd == -1, errno);
  if(fd == -1) {
    goto err_e;
  }
  loop->fd = fd;
  safe_execute(fd = pthread_mutex_init(&loop->lock, NULL), fd != 0, fd);
  if(fd != 0) {
    errno = fd;
    goto err_fd;
  }
  safe_execute(fd = eventfd(0, EFD_NONBLOCK | EFD_SEMAPHORE), fd == -1, errno);
  if(fd == -1) {
    goto err_mutex;
  }
  loop->evt.fd = fd;
  loop->evt.wakeup = 1;
  if(async_loop_add(loop, &loop->evt, EPOLLIN) != 0) {
    goto err_efd;
  }
  return 0;
  
  err_efd:
  (void) close(loop->evt.fd);
  err_mutex:
  (void) pthread_mutex_destroy(&loop->lock);
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
  atomic_thread_fence(memory_order_release);
  assert(!eventfd_write(loop->evt.fd, 1));
  (void) pthread_join(loop->thread, NULL);
}

void async_loop_free(struct async_loop* const loop) {
  (void) close(loop->fd);
  (void) close(loop->evt.fd);
  (void) pthread_mutex_destroy(&loop->lock);
  free(loop->fake_events);
  loop->fake_events = NULL;
  loop->fake_events_len = 0;
  free(loop->events);
  loop->events = NULL;
}

void async_loop_reset(struct async_loop* const loop) {
  atomic_store_explicit(&loop->evt.flags, 0, memory_order_relaxed);
}

void async_loop_push_joinable(struct async_loop* const loop) {
  atomic_fetch_or_explicit(&loop->evt.flags, 1, memory_order_relaxed);
}

void async_loop_push_free(struct async_loop* const loop) {
  atomic_fetch_or_explicit(&loop->evt.flags, 2, memory_order_relaxed);
}

void async_loop_push_ptr_free(struct async_loop* const loop) {
  atomic_fetch_or_explicit(&loop->evt.flags, 4, memory_order_relaxed);
}

void async_loop_commit(struct async_loop* const loop) {
  atomic_fetch_or_explicit(&loop->evt.flags, 8, memory_order_relaxed);
  atomic_thread_fence(memory_order_release);
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

static int async_loop_resize(struct async_loop* const loop, const uint32_t new_size) {
  void* ptr;
  safe_execute(ptr = realloc(loop->fake_events, sizeof(*loop->fake_events) * new_size), ptr == NULL, ENOMEM);
  if(ptr == NULL) {
    return -1;
  }
  loop->fake_events = ptr;
  return 0;
}

int async_loop_create_event(struct async_loop* const loop, struct async_event* const event) {
  (void) pthread_mutex_lock(&loop->lock);
  if(async_loop_resize(loop, loop->fake_events_len + 1) == -1) {
    (void) pthread_mutex_unlock(&loop->lock);
    return -1;
  }
  loop->fake_events[loop->fake_events_len++] = event;
  (void) pthread_mutex_unlock(&loop->lock);
  assert(!eventfd_write(loop->evt.fd, 1));
  return 0;
}

int async_loop_create_events(struct async_loop* const loop, struct async_event* const events, const uint32_t num) {
  (void) pthread_mutex_lock(&loop->lock);
  if(async_loop_resize(loop, loop->fake_events_len + num) == -1) {
    (void) pthread_mutex_unlock(&loop->lock);
    return -1;
  }
  (void) memcpy(loop->fake_events + loop->fake_events_len, events, sizeof(*events) * num);
  loop->fake_events_len += num;
  (void) pthread_mutex_unlock(&loop->lock);
  assert(!eventfd_write(loop->evt.fd, 1));
  return 0;
}