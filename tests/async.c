#include <shnet/tests.h>
#include <shnet/async.h>

void onevt(struct async_loop* loop, uint32_t events, struct async_event* event) {
  if(!events) {
    assert(!event);
    test_wake();
  }
}

struct async_loop loop = {0};

void* stop_loop(void* data) {
  test_sleep(10);
  async_loop_push_free(data);
  async_loop_commit(data);
  pthread_detach(pthread_self());
  return NULL;
}

int main() {
  begin_test("async 1");
  loop.on_event = onevt;
  loop.events_len = 1;
  assert(!async_loop(&loop));
  assert(!async_loop_start(&loop));
  async_loop_stop(&loop);
  end_test();
  
  begin_test("async 2");
  async_loop_free(&loop);
  assert(!async_loop(&loop));
  assert(!async_loop_start(&loop));
  async_loop_stop(&loop);
  end_test();
  
  begin_test("async 3");
  assert(!async_loop_start(&loop));
  async_loop_commit(&loop);
  test_sleep(10);
  end_test();
  
  begin_test("async sim event");
  assert(loop.on_event == onevt);
  assert(!async_loop_start(&loop));
  assert(!async_loop_create_event(&loop, NULL));
  test_wait();
  end_test();
  
  begin_test("async sim events");
  struct async_event events[13] = {0};
  assert(!async_loop_create_events(&loop, events, 13));
  for(int i = 0; i < 13; ++i) {
    test_wait();
  }
  end_test();
  
  begin_test("async stop");
  assert(loop.on_event == onevt);
  assert(loop.events_len == 1);
  async_loop_push_joinable(&loop);
  pthread_t t = loop.thread;
  async_loop_commit(&loop);
  (void) pthread_join(t, NULL);
  end_test();
  
  begin_test("async manual");
  async_loop_reset(&loop);
  assert(!pthread_start(NULL, stop_loop, &loop));
  async_loop_thread(&loop);
  end_test();
  return 0;
}