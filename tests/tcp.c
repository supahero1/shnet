#include "tests.h"

#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>
#include <semaphore.h>
#include <shnet/tcp.h>
#include <shnet/time.h>

sem_t sem;
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
uint64_t total = 0;

const uint64_t msg_size = 4096 << 7; /* 128 pages */
#define time_to_wait_ms 100UL

_Atomic int keep_sending;

struct tcp_socket_callbacks serversock_cb = {0};
struct tcp_socket_callbacks client_cb = {0};

struct tcp_server_callbacks server_cb = {0};

unsigned char* buf;

void serversock_onmessage(struct tcp_socket* socket) {
  errno = 0;
  uint64_t read = 0;
  while(atomic_load(&keep_sending) == 1 && errno == 0) {
    const uint64_t r = tcp_read(socket, buf, msg_size);
    if(r == 0) {
      break;
    }
    read += r;
  }
  total += read;
  if(read == 0 || atomic_load(&keep_sending) != 1) {
    return;
  }
  tcp_send(socket, buf, 1, tcp_read_only | tcp_dont_free);
}

void onopen(struct tcp_socket* socket) {
  sem_post(&sem);
}

void onmessage(struct tcp_socket* socket) {
  errno = 0;
  (void) tcp_read(socket, buf, msg_size);
  if(atomic_load(&keep_sending) != 1) {
    return;
  }
  tcp_send(socket, buf, msg_size, tcp_read_only | tcp_dont_free);
}

void onclose(struct tcp_socket* socket) {
  tcp_socket_free(socket);
  sem_post(&sem);
}

int sock_onnomem(struct tcp_socket* socket) {
  TEST_FAIL;
  return -1;
}

int onconnection(struct tcp_socket* socket, const struct sockaddr* addr) {
  socket->callbacks = &serversock_cb;
  return 0;
}

int onnomem(struct tcp_server* server) {
  TEST_FAIL;
  return -1;
}

void onshutdown(struct tcp_server* server) {
  tcp_server_free(server);
  sem_post(&sem);
}

void unlock_mutexo(void* da) {
  pthread_mutex_unlock(&mutex);
  atomic_store(&keep_sending, 0);
}

void too_long(void* a) {
  puts("too long");
  sleep(9999);
}

int main(int argc, char **argv) {
  _debug("Testing tcp:", 1);
  srand(time_get_time());
  sem_init(&sem, 0, 0);
  pthread_mutex_lock(&mutex);
  atomic_store(&keep_sending, 1);
  
  buf = calloc(1, msg_size);
  if(buf == NULL) {
    TEST_FAIL;
  }
  
  serversock_cb.onopen = onopen;
  serversock_cb.onmessage = serversock_onmessage;
  serversock_cb.onnomem = sock_onnomem;
  serversock_cb.onclose = onclose;
  
  client_cb.onopen = onopen;
  client_cb.onmessage = onmessage;
  client_cb.onnomem = sock_onnomem;
  client_cb.onclose = onclose;
  
  server_cb.onconnection = onconnection;
  server_cb.onnomem = onnomem;
  server_cb.onshutdown = onshutdown;
  
  const int number = atoi(argv[1]);
  
  const struct addrinfo hints = net_get_addr_struct(ipv4, stream_socktype, tcp_protocol, 0);
  struct addrinfo* res = net_get_address("localhost", "5000", &hints);
  if(res == NULL) {
    _debug("net_get_address() err %d %s\n", 1, errno, strerror(errno));
    TEST_FAIL;
  }
  
  /* TCP server setup */
  struct tcp_server* servers = calloc(number, sizeof(struct tcp_server));
  if(servers == NULL) {
    TEST_FAIL;
  }
  for(int i = 0; i < number; ++i) {
    servers[i].callbacks = &server_cb;
    if(tcp_server(&servers[i], &((struct tcp_server_options) {
      .info = res,
      .hostname = "localhost",
      .port = "5000",
      .family = ipv4,
      .flags = 0
    })) != 0) {
      TEST_FAIL;
    }
  }
  
  /* TCP socket setup */
  struct tcp_socket* sockets = calloc(number, sizeof(struct tcp_socket));
  if(sockets == NULL) {
    TEST_FAIL;
  }
  for(int i = 0; i < number; ++i) {
    sockets[i].epoll = servers[i].epoll;
    sockets[i].callbacks = &client_cb;
    sockets[i].info = res;
    if(tcp_socket(&sockets[i], NULL) != 0) {
      TEST_FAIL;
    }
  }
  
  /* Time setup to stop the test */
  struct time_manager manager = {0};
  if(time_manager(&manager) != 0) {
    TEST_FAIL;
  }
  if(time_manager_start(&manager) != 0) {
    TEST_FAIL;
  }
  for(int i = 0; i < (number << 1); ++i) {
    sem_wait(&sem);
  }
  if(time_manager_add_timeout(&manager, time_get_ms(time_to_wait_ms), unlock_mutexo, NULL, NULL) != 0) {
    TEST_FAIL;
  }
  if(time_manager_add_timeout(&manager, time_get_ms(time_to_wait_ms << 1), too_long, NULL, NULL) != 0) {
    TEST_FAIL;
  }
  for(int i = 0; i < number; ++i) {
    tcp_send(&sockets[i], buf, msg_size, tcp_read_only | tcp_dont_free);
  }
  /* Wait for the timeout to expire, we will be sending data in the background */
  pthread_mutex_lock(&mutex);
  /* Close the sockets */
  for(int i = 0; i < number; ++i) {
    tcp_socket_force_close(sockets + i);
  }
  for(int i = 0; i < (number << 1); ++i) {
    sem_wait(&sem);
  }
  /* Close the servers */
  for(int i = 0; i < number; ++i) {
    if(tcp_server_shutdown(&servers[i]) != 0) {
      TEST_FAIL;
    }
    sem_wait(&sem);
  }
  /* The test is over */
  _debug("The results:\n"
  "Cores used : %d\n"
  "Data size  : %lu\n"
  "B in %lu ms: %lu\n"
  "Throughput : %lu b/s, %lu kb/s, %lu mb/s, %lu gb/s",
  1,
  number,
  msg_size,
  time_to_wait_ms,
  total,
  total * 1000U / time_to_wait_ms,
  total / time_to_wait_ms,
  total / (time_to_wait_ms * 1000U),
  total / (time_to_wait_ms * 1000000U)
  );
  pthread_mutex_unlock(&mutex);
  pthread_mutex_destroy(&mutex);
  sem_destroy(&sem);
  free(sockets);
  free(servers);
  time_manager_stop(&manager);
  time_manager_free(&manager);
  net_free_address(res);
  free(buf);
  TEST_PASS;
  return 0;
}