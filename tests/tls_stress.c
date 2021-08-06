#include "tests.h"

#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>
#include <semaphore.h>
#include <shnet/tls.h>
#include <shnet/time.h>

sem_t sem;
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

_Atomic uint64_t conns = 0;

#define time_to_wait_ms 100UL

struct tls_socket_callbacks serversock_cb = {0};
struct tls_socket_callbacks clientsock_cb = {0};
struct tls_socket_settings clientsock_set = { 1, 1, 0, 1 };

struct tls_server_callbacks server_cb = {0};

void serversock_onopen(struct tls_socket* socket) {
  tls_socket_force_close(socket);
}

void serversock_onclose(struct tls_socket* socket) {
  tls_socket_free(socket);
}

void onopen(struct tls_socket* socket) {
  tcp_socket_keepalive_explicit(&socket->tcp, 1, 1, 1);
  atomic_fetch_add(&conns, 1);
}

void onclose(struct tls_socket* socket) {
  tls_socket_free(socket);
  sem_post(&sem);
}

int sock_onnomem(struct tls_socket* socket) {
  TEST_FAIL;
  return -1;
}

int onconnection(struct tls_socket* socket, const struct sockaddr* addr) {
  socket->callbacks = &serversock_cb;
  return 0;
}

int onnomem(struct tls_server* server) {
  TEST_FAIL;
  return -1;
}

void onshutdown(struct tls_server* server) {
  (void) close(server->tcp.net.net.sfd);
  server->tcp.net.net.sfd = -1;
  sem_post(&sem);
}

void unlock_mutexo(void* da) {
  pthread_mutex_unlock(&mutex);
}

int main(int argc, char **argv) {
  _debug("Testing tls_stress:", 1);
  srand(time_get_time());
  sem_init(&sem, 0, 0);
  pthread_mutex_lock(&mutex);
  tls_ignore_sigpipe();
  
  serversock_cb.onopen = serversock_onopen;
  serversock_cb.onnomem = sock_onnomem;
  serversock_cb.onclose = serversock_onclose;
  
  clientsock_cb.onopen = onopen;
  clientsock_cb.onnomem = sock_onnomem;
  clientsock_cb.onclose = onclose;
  
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
  
  /* TLS server setup */
  struct tls_server* servers = calloc(number, sizeof(struct tls_server));
  if(servers == NULL) {
    TEST_FAIL;
  }
  for(int i = 0; i < number; ++i) {
    servers[i].callbacks = &server_cb;
    if(tls_server(&servers[i], &((struct tls_server_options) {
      .tcp = (struct tcp_server_options) {
        .info = res,
        .hostname = "localhost",
        .port = "5000",
        .family = ipv4,
        .flags = 0
      },
      .cert_path = "./tests/cert.pem",
      .key_path = "./tests/key.pem",
      .flags = tls_rsa_key
    })) != 0) {
      TEST_FAIL;
    }
  }
  
  /* TLS socket setup */
  struct tls_socket* sockets = calloc(number, sizeof(struct tls_socket));
  if(sockets == NULL) {
    TEST_FAIL;
  }
  for(int i = 0; i < number; ++i) {
    sockets[i].tcp.epoll = servers[i].tcp.epoll;
    sockets[i].callbacks = &clientsock_cb;
    sockets[i].settings = clientsock_set;
    sockets[i].tcp.info = res;
    if(tls_socket(&sockets[i], &((struct tls_socket_options) {
      .tcp = (struct tcp_socket_options) {
        .hostname = "localhost",
        .port = "5000",
        .family = ipv4,
        .flags = 0
      }
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
  /* Shutdown the servers to close existing connections. By design, the clients
  will keep on trying to reconnect until they realise they have tried every
  available mean of doing so, and then will finally stop. */
  for(int i = 0; i < number; ++i) {
    if(tls_server_shutdown(&servers[i]) != 0) {
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
    net_epoll_stop(servers[i].tcp.epoll);
    net_epoll_free(servers[i].tcp.epoll);
    free(servers[i].tcp.epoll);
    servers[i].tcp.epoll = NULL;
    servers[i].tcp.alloc_epoll = 0;
    tls_server_free(&servers[i]);
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