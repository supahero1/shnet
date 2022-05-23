#include <shnet/test.h>

#include <unistd.h>
#include <stdlib.h>
#include <stdatomic.h>
#include <sys/eventfd.h>

#include <shnet/async.h>

void onevt(struct async_loop* loop, uint32_t events, struct async_event* event) {
  uint64_t out;
  assert(!eventfd_read(event->fd, &out));
  assert(events == EPOLLIN);
  assert(out == (uintptr_t) event);
  test_wake();
}

test_register(void*, shnet_malloc, (const size_t a), (a))
test_register(int, eventfd, (unsigned int a, int b), (a, b))
test_register(int, epoll_create1, (int a), (a))
test_register(int, epoll_ctl, (int a, int b, int c, struct epoll_event* d), (a, b, c, d))
test_register(int, pthread_create, (pthread_t* a, const pthread_attr_t* b, void* (*c)(void*), void* d), (a, b, c, d))

int main() {
  test_begin("async check");
  test_error_check(void*, shnet_malloc, (0xbad));
  test_error_check(int, eventfd, (0xbad, 0xbad));
  test_error_check(int, epoll_create1, (0xbad));
  test_error_check(int, epoll_ctl, (0xbad, 0xbad, 0xbad, (void*) 0xbad));
  test_error_check(int, pthread_create, ((void*) 0xbad, (void*) 0xbad, (void*) 0xbad, (void*) 0xbad));
  
  test_error_set_retval(shnet_malloc, NULL);
  test_error_set_retval(pthread_create, ECANCELED);
  test_end();
  
  test_begin("async init err 1");
  test_error(shnet_malloc);
  struct async_loop loop = {0};
  assert(async_loop(&loop));
  test_end();
  
  test_begin("async init err 2");
  test_error(epoll_create1);
  assert(async_loop(&loop));
  test_end();
  
  test_begin("async init err 3");
  test_error(eventfd);
  assert(async_loop(&loop));
  test_end();
  
  test_begin("async init err 4");
  test_error(epoll_ctl);
  assert(async_loop(&loop));
  test_end();
  
  test_begin("async init");
  assert(!async_loop(&loop));
  async_loop_free(&loop);
  assert(!async_loop(&loop));
  test_end();
  
  test_begin("async start err");
  test_error(pthread_create);
  assert(async_loop_start(&loop));
  test_end();
  
  test_begin("async start");
  assert(!async_loop_start(&loop));
  test_end();
  
  test_begin("async stop");
  async_loop_stop(&loop);
  async_loop_free(&loop);
  assert(!async_loop(&loop));
  assert(!async_loop_start(&loop));
  test_end();
  
  test_begin("async stop async");
  async_loop_push_free(&loop);
  async_loop_commit(&loop);
  test_end();
  
  test_begin("async stop async free");
  struct async_loop* loop2 = calloc(1, sizeof(*loop2));
  assert(loop2);
  assert(!async_loop(loop2));
  assert(!async_loop_start(loop2));
  async_loop_push_joinable(loop2);
  async_loop_push_free(loop2);
  async_loop_push_ptr_free(loop2);
  pthread_t save = loop2->thread;
  async_loop_commit(loop2);
  pthread_join(save, NULL);
  test_end();
  
  test_begin("async event init");
  struct async_loop l = {0};
  l.events_len = 2;
  l.on_event = onevt;
  assert(!async_loop(&l));
  assert(!async_loop_start(&l));
  struct async_event events[5] = {0};
  for(int i = 0; i < 5; ++i) {
    events[i].fd = eventfd(0, EFD_NONBLOCK);
    assert(events[i].fd != -1);
    assert(!async_loop_add(&l, events + i, EPOLLIN | EPOLLET));
  }
  test_end();
  
  test_begin("async event 1");
  for(int i = 0; i < 5; ++i) {
    assert(!eventfd_write(events[i].fd, (uintptr_t)(events + i)));
  }
  for(int i = 0; i < 5; ++i) {
    test_wait();
  }
  test_end();
  
  test_begin("async event 2");
  assert(!async_loop_mod(&l, events + 0, 0));
  assert(!eventfd_write(events[0].fd, 1));
  for(int i = 1; i < 5; ++i) {
    assert(!eventfd_write(events[i].fd, (uintptr_t)(events + i)));
  }
  for(int i = 0; i < 4; ++i) {
    test_wait();
  }
  test_end();
  
  test_begin("async event 3");
  assert(!eventfd_write(events[0].fd, (uintptr_t) events - 1));
  assert(!async_loop_mod(&l, events + 0, EPOLLIN));
  test_wait();
  test_end();
  
  test_begin("async manual");
  async_loop_stop(&l);
  async_loop_commit(&l);
  (void) async_loop_thread(&l);
  test_end();
  
  test_begin("async event free");
  for(int i = 0; i < 5; ++i) {
    assert(!async_loop_remove(&l, events + i));
    close(events[i].fd);
  }
  async_loop_free(&l);
  test_end();
  
  return 0;
}
