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

_Atomic uint64_t conns = 0;

#define time_to_wait_ms 100UL

struct tcp_socket_callbacks clientsock_cb = {0};
struct tcp_socket_settings clientsock_set = { 1, 0, 1, 1, 0, 0, 1, 0 };

struct tcp_server_callbacks server_cb = {0};

void onopen(struct tcp_socket* socket) {
  tcp_socket_keepalive_explicit(socket, 1, 1, 1);
  atomic_fetch_add(&conns, 1);
}

void onclose(struct tcp_socket* socket) {
  tcp_socket_free(socket);
  sem_post(&sem);
}

int sock_onnomem(struct tcp_socket* socket) {
  TEST_FAIL;
  return -1;
}

int onnomem(struct tcp_server* server) {
  TEST_FAIL;
  return -1;
}

void onshutdown(struct tcp_server* server) {
  (void) close(server->net.net.sfd);
  server->net.net.sfd = -1;
  sem_post(&sem);
}

void unlock_mutexo(void* da) {
  pthread_mutex_unlock(&mutex);
}

int main(int argc, char **argv) {
  _debug("Testing tcp_stress:", 1);
  srand(time_get_time());
  sem_init(&sem, 0, 0);
  pthread_mutex_lock(&mutex);
  
  clientsock_cb.onopen = onopen;
  clientsock_cb.onnomem = sock_onnomem;
  clientsock_cb.onclose = onclose;
  
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
      .info = res
    })) != 0) {
      TEST_FAIL;
    }
    tcp_server_dont_accept_conn(servers + i);
  }
  
  /* TCP socket setup */
  struct tcp_socket* sockets = calloc(number, sizeof(struct tcp_socket));
  if(sockets == NULL) {
    TEST_FAIL;
  }
  for(int i = 0; i < number; ++i) {
    sockets[i].epoll = servers[i].epoll;
    sockets[i].callbacks = &clientsock_cb;
    sockets[i].settings = clientsock_set;
    sockets[i].info = res;
    if(tcp_socket(&sockets[i], &((struct tcp_socket_options) {
      .hostname = "localhost",
      .port = "5000",
      .family = ipv4,
      .flags = 0
    })) != 0) {
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
  if(time_manager_add_timeout(&manager, time_get_ms(time_to_wait_ms), unlock_mutexo, NULL, NULL) != 0) {
    TEST_FAIL;
  }
  /* Wait for the timeout to expire */
  pthread_mutex_lock(&mutex);
  for(int i = 0; i < number; ++i) {
    if(tcp_server_shutdown(&servers[i]) != 0) {
      TEST_FAIL;
    }
    sem_wait(&sem);
  }
  for(int i = 0; i < number; ++i) {
    sem_wait(&sem);
  }
  /* The test is over */
  _debug("The results:\n"
  "Number used : %d\n"
  "No. accepted: %lu\n"
  "Throughput  : %lu clients/second",
  1,
  number,
  atomic_load(&conns),
  atomic_load(&conns) * 1000U / time_to_wait_ms
  );
  for(int i = 0; i < number; ++i) {
    net_epoll_stop(servers[i].epoll);
    net_epoll_free(servers[i].epoll);
    free(servers[i].epoll);
    servers[i].epoll = NULL;
    servers[i].alloc_epoll = 0;
    tcp_server_free(&servers[i]);
  }
  pthread_mutex_unlock(&mutex);
  pthread_mutex_destroy(&mutex);
  sem_destroy(&sem);
  free(sockets);
  free(servers);
  time_manager_stop(&manager);
  time_manager_free(&manager);
  net_free_address(res);
  usleep(1000);
  TEST_PASS;
  return 0;
}