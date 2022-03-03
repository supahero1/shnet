#include <shnet/tests.h>

#include <fcntl.h>
#include <sched.h>
#include <stdint.h>
#include <string.h>

#include <shnet/tcp.h>

struct tcp_socket _socket = {0};
struct tcp_socket _socket2 = {0};
struct tcp_socket* serversock;
struct tcp_server server = {0};

struct async_loop loop = {0};

char async_payload[4096];
char async_readbuf[4096 << 1];
uint64_t async_payload_read = 0;
uint64_t wants_to_read = sizeof(async_payload);

int server_event_number = 0;

void sock_open_close_event(struct tcp_socket* sock, enum tcp_event event) {
  switch(event) {
    case tcp_open:
    case tcp_free: test_wake();
    default: break;
  }
}

void socket_close_only_event(struct tcp_socket* sock, enum tcp_event event) {
  switch(event) {
    case tcp_open: assert(0);
    case tcp_close: {
      tcp_socket_free(sock);
      break;
    }
    case tcp_free: test_wake();
    default: break;
  }
}

void sock_freeonly_event(struct tcp_socket* sock, enum tcp_event event) {
  switch(event) {
    case tcp_free: {
      test_wake();
    }
    default: break;
  }
}

void serversock_async_event(struct tcp_socket* sock, enum tcp_event event) {
  switch(event) {
    case tcp_data: {
      uint64_t read = tcp_read(sock, async_readbuf + async_payload_read, sizeof(async_readbuf) - async_payload_read);
      async_payload_read += read;
      assert(async_payload_read <= wants_to_read);
      if(read != 0 && async_payload_read == wants_to_read) {
        tcp_socket_free(sock);
      }
      break;
    }
    case tcp_free: test_wake();
    default: break;
  }
}

void serversock_no_data_event(struct tcp_socket* sock, enum tcp_event event) {
  switch(event) {
    case tcp_data: {
      uint64_t read = tcp_read(sock, async_readbuf + async_payload_read, sizeof(async_readbuf) - async_payload_read);
      async_payload_read += read;
      assert(async_payload_read <= wants_to_read);
      if(read != 0 && async_payload_read == wants_to_read) {
        tcp_socket_free(sock);
      }
      break;
    }
    case tcp_close: {
      tcp_socket_free(sock);
      break;
    }
    case tcp_open:
    case tcp_free: test_wake();
    default: break;
  }
}

struct tcp_socket* server_event(struct tcp_server* serv, enum tcp_event event, struct tcp_socket* sock) {
  switch(event) {
    case tcp_open: {
      switch(server_event_number) {
        case 0: {
          sock->on_event = serversock_async_event;
          break;
        }
        case 1: {
          sock->on_event = serversock_no_data_event;
          break;
        }
        default: assert(0);
      }
      break;
    }
    case tcp_close: {
      tcp_server_free(serv);
      break;
    }
    case tcp_free: {
      test_wake();
      break;
    }
    default: break;
  }
  return sock;
}

test_register(int, listen, (int a, int b), (a, b))
test_register(int, getaddrinfo, (const char* a, const char* b, const struct addrinfo* c, struct addrinfo** d), (a, b, c, d))

int main() {
  test_seed_random();
  for(int i = 0; i < 4096; ++i) {
    async_payload[i] = rand();
  }
  _socket.loop = &loop;
  _socket2.loop = &loop;
  
  begin_test("tcp init");
  assert(!tcp_async_loop(&loop));
  assert(!async_loop_start(&loop));
  
  server.on_event = server_event;
  server.loop = &loop;
  test_error(listen);
  assert(tcp_server(&server, &((struct tcp_server_options) {
    .hostname = "127.0.0.1",
    .port = "0",
    .family = net_family_ipv4,
    .backlog = 2
  })));
  assert(server.on_event == server_event);
  assert(server.loop == &loop);
  assert(!tcp_server(&server, &((struct tcp_server_options) {
    .hostname = "127.0.0.1",
    .port = "0",
    .family = net_family_ipv4,
    .backlog = 2
  })));
  /*
   * Don't allow double initialisation.
   */
  assert(tcp_server(&server, &((struct tcp_server_options) {
    .hostname = "127.0.0.1",
    .port = "0",
    .family = net_family_ipv4,
    .backlog = 2
  })));
  assert(errno == EINVAL);
  
  char port_str[6] = {0};
  const uint16_t port = tcp_server_get_port(&server);
  assert(sprintf(port_str, "%hu", port) > 0);
  const struct addrinfo hints = net_get_addr_struct(net_family_ipv4, net_sock_stream, net_proto_tcp, 0);
  struct addrinfo* const info = net_get_address("127.0.0.1", port_str, &hints);
  end_test();
  
  begin_test("tcp async 1");
  /*
   * This test has a DNS lookup performed by the tcp_socket() call,
   * which in turn makes tcp_send() finish (likely) before the DNS
   * lookup is done, and thus before opening the socket. Since this
   * is localhost, the chances of a socket opening semi-instantly
   * are quite high. This is meant to prevent it to test more.
   */
  _socket.on_event = sock_freeonly_event;
  assert(!tcp_socket(&_socket, &((struct tcp_socket_options) {
    .hostname = "127.0.0.1",
    .port = port_str,
    .family = net_family_ipv4
  })));
  assert(!tcp_send(&_socket, &((struct data_frame) {
    .data = async_payload,
    .len = sizeof(async_payload),
    .dont_free = 1,
    .read_only = 1
  })));
  tcp_socket_close(&_socket);
  tcp_socket_free(&_socket);
  test_wait();
  test_wait();
  assert(async_payload_read == sizeof(async_payload));
  assert(memcmp(async_payload, async_readbuf, sizeof(async_payload)) == 0);
  end_test();
  
  begin_test("tcp async 2");
  /*
   * This test doesn't have a DNS lookup delay.
   */
  _socket.on_event = sock_freeonly_event;
  async_payload_read = 0;
  assert(!tcp_socket(&_socket, &((struct tcp_socket_options) {
    .info = info
  })));
  assert(!tcp_send(&_socket, &((struct data_frame) {
    .data = async_payload,
    .len = sizeof(async_payload),
    .dont_free = 1,
    .read_only = 1
  })));
  tcp_socket_close(&_socket);
  tcp_socket_free(&_socket);
  test_wait();
  test_wait();
  assert(async_payload_read == sizeof(async_payload));
  assert(memcmp(async_payload, async_readbuf, sizeof(async_payload)) == 0);
  end_test();
  
  begin_test("tcp async 3");
  /*
   * Send byte by byte, doesn't hurt to see what happens.
   * Make these bytes not clamp together by yielding and
   * turning off Nagle's algorithm (big difference!).
   * Don't pay attention to high test memory usage. It's
   * caused by some Valgrind versions counting all of
   * realloc()'s as free() + malloc(), thus adding up to
   * enormous amounts of memory with a lot of them.
   */
  _socket.on_event = sock_freeonly_event;
  async_payload_read = 0;
  assert(!tcp_socket(&_socket, &((struct tcp_socket_options) {
    .info = info
  })));
  tcp_socket_nodelay_on(&_socket);
  for(unsigned int i = 0; i < sizeof(async_payload); ++i) {
    assert(!tcp_send(&_socket, &((struct data_frame) {
      .data = async_payload,
      .offset = i,
      .len = i + 1,
      .dont_free = 1,
      .read_only = 1
    })));
    sched_yield();
  }
  tcp_socket_close(&_socket);
  tcp_socket_free(&_socket);
  test_wait();
  test_wait();
  assert(async_payload_read == sizeof(async_payload));
  assert(memcmp(async_payload, async_readbuf, sizeof(async_payload)) == 0);
  end_test();
  
  begin_test("tcp async 4");
  /*
   * The socket is opened with a delay (via a DNS lookup), data
   * to be sent is enqueued, but should not be processed when
   * forcibly closing the connection. The server should note
   * a new connection and accept it.
   * There's a very slight chance that this test fails and/or
   * triggers undefined behavior for the rest of the test. That
   * will happen when this thread gets preempted, the DNS lookup
   * completes and at the same time the scheduler switches to
   * the epoll thread to open the socket and start sending data.
   * That's a rather miraculous turn of events and has never
   * happened in my tests yet, this is only theory.
   */
  _socket.on_event = sock_open_close_event;
  async_payload_read = 0;
  server_event_number = 1;
  assert(!tcp_socket(&_socket, &((struct tcp_socket_options) {
    .hostname = "127.0.0.1",
    .port = port_str,
    .family = net_family_ipv4
  })));
  assert(!tcp_send(&_socket, &((struct data_frame) {
    .data = async_payload,
    .len = sizeof(async_payload),
    .dont_free = 1,
    .read_only = 1
  })));
  tcp_socket_force_close(&_socket);
  tcp_socket_free(&_socket);
  test_wait();
  test_wait();
  test_wait();
  test_wait();
  assert(async_payload_read == 0);
  server_event_number = 0;
  end_test();
  
  begin_test("tcp zero copy send");
  _socket.on_event = sock_freeonly_event;
  async_payload_read = 0;
  wants_to_read = 4;
  
  char* _dir = get_current_dir_name();
  assert(_dir);
  const size_t len = strlen(_dir);
  const size_t len2 = strlen("/tests/test.txt");
  char* dir = malloc(len + len2 + 1);
  assert(dir);
  memcpy(dir, _dir, len);
  memcpy(dir + len, "/tests/test.txt", len2 + 1);
  
  int file = open(dir, 0);
  /*
   * Make sure to execute "make test"
   * from the root directory (above this).
   */
  assert(file != -1);
  
  assert(!tcp_socket(&_socket, &((struct tcp_socket_options) {
    .info = info
  })));
  assert(!tcp_send(&_socket, &((struct data_frame) {
    .fd = file,
    .offset = 0,
    .len = 4,
    .read_only = 1,
    .file = 1
  })));
  tcp_socket_close(&_socket);
  tcp_socket_free(&_socket);
  test_wait();
  test_wait();
  assert(async_payload_read == 4);
  assert(memcmp(async_readbuf, "test", 4) == 0);
  free(dir);
  free(_dir);
  end_test();
  
  begin_test("tcp close");
  /*
   * Required for the next test.
   */
  assert(!tcp_server_close(&server));
  test_wait();
  end_test();
  
  begin_test("tcp err 1");
  /*
   * Connect to a port no one is listening on (hopefully). Delay
   * connection with a DNS lookup to not fail in tcp_socket().
   * Unlike previous tests, the tcp_open event is not fired here.
   */
  _socket.on_event = socket_close_only_event;
  _socket.dont_send_buffered = 1;
  assert(!tcp_socket(&_socket, &((struct tcp_socket_options) {
    .hostname = "127.0.0.1",
    .port = port_str,
    .family = net_family_ipv4
  })));
  test_wait();
  end_test();
  
  begin_test("tcp err 2");
  /*
   * Make tcp_socket() succeed, but getaddrinfo() fail. tcp_open
   * not fired.
   */
  _socket.on_event = socket_close_only_event;
  test_error(getaddrinfo);
  assert(!tcp_socket(&_socket, &((struct tcp_socket_options) {
    .hostname = "127.0.0.1",
    .port = port_str,
    .family = net_family_ipv4
  })));
  test_wait();
  end_test();
  
  /*
   * All of the above are also testing structure reuse. A socket
   * structure can be reused immediatelly after tcp_socket_free().
   */
  
  begin_test("tcp consistency checks");
  /*
   * tcp_socket_free() shall not zero useful information.
   * Some information should always be zeroed.
   * Some information is only zeroed upon next initialisation.
   */
  assert(_socket.loop == &loop);
  assert(_socket.on_event == socket_close_only_event);
  assert(_socket.alloc_loop == 0);
  assert(_socket.opened == 0);
  assert(_socket.close_guard == 0);
  assert(_socket.dont_send_buffered == 1);
  assert(_socket.dont_close_onreadclose == 0);
  end_test();
  
  begin_test("tcp free");
  async_loop_stop(&loop);
  async_loop_free(&loop);
  net_free_address(info);
  end_test();
  
  return 0;
}