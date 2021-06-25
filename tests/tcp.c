#include "tests.h"

#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <semaphore.h>
#include <shnet/tcp.h>
#include <shnet/time.h>

/* This test suite was inspired by https://github.com/chronoxor/CppServer#tcp-echo-server
which doesn't even respond to single pings, just resends whatever it received.
Unlike the CppServer library, we don't lose performance by using more clients, since
there is no lock contention or anything that would stop the clients other than the
machine's power. */

sem_t sem;
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
unsigned long msges = 0;

#define how_many_pings 1000UL
#define ping_size 32UL
#define time_to_wait_s 1UL

struct tcp_socket_callbacks serversock_cb;
struct tcp_socket_settings serversock_set;

struct tcp_server_callbacks server_cb;
struct tcp_server_settings server_set;

void serversock_onopen(struct tcp_socket* socket) {
  (void) socket;
  sem_post(&sem);
}

void serversock_onmessage(struct tcp_socket* socket) {
  unsigned char buf[ping_size * how_many_pings];
  errno = 0;
  while(errno == 0) {
    int read = tcp_read(socket, buf, ping_size * how_many_pings);
    if(read == 0) {
      return;
    }
    const int pings = read / ping_size;
    msges += pings;
    (void) tcp_send(socket, buf, ping_size * pings);
  }
}

void serversock_onclose(struct tcp_socket* socket) {
  sem_post(&sem);
  tcp_socket_confirm_close(socket);
}



void onopen(struct tcp_socket* socket) {
  (void) socket;
  sem_post(&sem);
}

void onmessage(struct tcp_socket* socket) {
  unsigned char buf[ping_size * how_many_pings];
  errno = 0;
  while(errno == 0) {
    int read = tcp_read(socket, buf, ping_size * how_many_pings);
    if(read == 0) {
      return;
    }
    const int pings = read / ping_size;
    (void) tcp_send(socket, buf, ping_size * pings);
  }
}

void onclose(struct tcp_socket* socket) {
  sem_post(&sem);
  tcp_socket_confirm_close(socket);
}



int onconnection(struct tcp_server* server, struct tcp_socket* socket) {
  socket->callbacks = &serversock_cb;
  socket->settings = &serversock_set;
  return tcp_proceed;
}

int onnomem(struct tcp_server* server) {
  TEST_FAIL;
  return tcp_fatal;
}

void onshutdown(struct tcp_server* server) {
  tcp_server_free(server);
  sem_post(&sem);
}

#define server ((struct tcp_server*) data)

int server_pick_address(struct addrinfo* info, void* data) {
  char ip[ip_max_strlen];
  net_sockbase_set_family(&server->base, net_addrinfo_get_family(info));
  net_sockbase_set_whole_addr(&server->base, net_addrinfo_get_whole_addr(info));
  (void) net_address_to_string(net_sockbase_get_whole_addr(&server->base), ip);
  if(tcp_create_server(server) != net_success) {
    printf("server_pick_address() err %s errno %d\n", net_strerror(errno), errno);
    return net_failure;
  } else {
    return net_success;
  }
}

#undef server
#define socket ((struct tcp_socket*) data)

int socket_pick_address(struct addrinfo* info, void* data) {
  char ip[ip_max_strlen];
  net_sockbase_set_family(&socket->base, net_addrinfo_get_family(info));
  net_sockbase_set_whole_addr(&socket->base, net_addrinfo_get_whole_addr(info));
  (void) net_address_to_string(net_sockbase_get_whole_addr(&socket->base), ip);
  if(tcp_create_socket(socket) != net_success) {
    printf("socket_pick_address() err %s errno %d\n", net_strerror(errno), errno);
    return net_failure;
  } else {
    return net_success;
  }
}

#undef socket

void unlock_mutexo(void* da) {
  (void) da;
  pthread_mutex_unlock(&mutex);
}

int main(int argc, char **argv) {
  puts("Testing tcp.c:");
  srand(time_get_time());
  sem_init(&sem, 0, 0);
  pthread_mutex_lock(&mutex);
  
  memset(&serversock_cb, 0, sizeof(serversock_cb));
  serversock_cb.onopen = serversock_onopen;
  serversock_cb.onmessage = serversock_onmessage;
  serversock_cb.onclose = serversock_onclose;
  
  memset(&serversock_set, 0, sizeof(serversock_set));
  serversock_set.send_buffer_cleanup_threshold = UINT_MAX; /* Never cleanup (only free) */
  serversock_set.send_buffer_allow_freeing = 0; /* Only free on socket free */
  /* Maybe the kernel has smaller default TCP window size; it is probably wise to
  not disable the send buffer */
  serversock_set.disable_send_buffer = 0;
  /* Too lazy to do it ourselves + nothing to do on onreadclose event anyway */
  serversock_set.onreadclose_auto_res = 1;
  
  memset(&server_cb, 0, sizeof(server_cb));
  server_cb.onconnection = onconnection;
  server_cb.onnomem = onnomem;
  server_cb.onshutdown = onshutdown;
  
  memset(&server_set, 0, sizeof(server_set));
  server_set.max_conn = 10000;
  server_set.backlog = 128;
  
  const int nclients = atoi(argv[1]);
  const int nservers = atoi(argv[2]);
  const int shared_epoll = atoi(argv[3]);
  
  /* Epoll setup */
  struct net_epoll socket_epoll;
  memset(&socket_epoll, 0, sizeof(socket_epoll));
  int err = tcp_epoll(&socket_epoll);
  if(err == net_failure) {
    TEST_FAIL;
  }
  if(net_epoll_start(&socket_epoll, 1) != net_success) {
    TEST_FAIL;
  }
  
  struct net_epoll server_epoll;
  if(shared_epoll == 0) {
    memset(&server_epoll, 0, sizeof(server_epoll));
    err = tcp_epoll(&server_epoll);
    if(err == net_failure) {
      TEST_FAIL;
    }
    if(net_epoll_start(&server_epoll, 1) != net_success) {
      TEST_FAIL;
    }
  }
  
  struct addrinfo hints = net_get_addr_struct(any_family, stream_socktype, tcp_protocol, numeric_service | wants_a_server | wants_own_ip_version);
  struct addrinfo* res = net_get_address(NULL, "8099", &hints);
  if(res == NULL) {
    printf("err %s\n", net_strerror(errno));
    TEST_FAIL;
  }
  
  /* TCP server setup */
  struct tcp_server* servers = malloc(sizeof(struct tcp_server) * nservers);
  if(servers == NULL) {
    TEST_FAIL;
  }
  memset(servers, 0, sizeof(struct tcp_server) * nservers);
  for(int i = 0; i < nservers; ++i) {
    if(shared_epoll == 1) {
      servers[i].epoll = &socket_epoll;
    } else {
      servers[i].epoll = &server_epoll;
    }
    servers[i].settings = &server_set;
    servers[i].callbacks = &server_cb;
    if(net_foreach_addrinfo(res, server_pick_address, &servers[i]) == net_failure) {
      TEST_FAIL;
    }
  }
  net_get_address_free(res);
  
  /* TCP socket setup */
  struct tcp_socket_callbacks sock_cb;
  memset(&sock_cb, 0, sizeof(sock_cb));
  sock_cb.onopen = onopen;
  sock_cb.onmessage = onmessage;
  sock_cb.onclose = onclose;
  
  hints = net_get_addr_struct(any_family, stream_socktype, tcp_protocol, numeric_service | numeric_hostname | wants_own_ip_version);
  res = net_get_address("0.0.0.0", "8099", &hints);
  if(res == NULL) {
    printf("err %s\n", net_strerror(errno));
    TEST_FAIL;
  }
  
  struct tcp_socket* sockets = malloc(sizeof(struct tcp_socket) * nclients);
  if(servers == NULL) {
    TEST_FAIL;
  }
  memset(sockets, 0, sizeof(struct tcp_socket) * nclients);
  for(int i = 0; i < nclients; ++i) {
    sockets[i].epoll = &socket_epoll;
    sockets[i].callbacks = &sock_cb;
    sockets[i].settings = &serversock_set;
    
    /* Pick an address for the socket */
    if(net_foreach_addrinfo(res, socket_pick_address, &sockets[i]) == net_failure) {
      TEST_FAIL;
    }
  }
  net_get_address_free(res);
  
  /* Time setup to stop the test */
  struct time_manager manager;
  err = time_manager(&manager, 1, 1);
  if(err != time_success) {
    TEST_FAIL;
  }
  err = time_manager_start(&manager);
  if(err != time_success) {
    TEST_FAIL;
  }
  for(int i = 0; i < (nclients << 1); ++i) {
    sem_wait(&sem);
  }
  err = time_manager_add_timeout(&manager, time_get_sec(time_to_wait_s), unlock_mutexo, NULL, NULL);
  if(err != time_success) {
    TEST_FAIL;
  }
  unsigned char buf[ping_size * how_many_pings];
  for(unsigned long i = 0; i < ping_size * how_many_pings; ++i) {
    buf[i] = rand();
  }
  for(int i = 0; i < nclients; ++i) {
    errno = 0;
    (void) tcp_send(&sockets[i], buf, ping_size * how_many_pings);
  }
  /* Wait for the timeout to expire, we will be sending data in the background */
  pthread_mutex_lock(&mutex);
  /* Close the clients */
  for(int i = 0; i < nclients; ++i) {
    if(tcp_socket_force_close(&sockets[i]) != net_success) {
      TEST_FAIL;
    }
  }
  for(int i = 0; i < (nclients << 1); ++i) {
    sem_wait(&sem);
  }
  /* Close the servers */
  for(int i = 0; i < nservers; ++i) {
    if(tcp_server_shutdown(&servers[i]) != net_success) {
      TEST_FAIL;
    }
    sem_wait(&sem);
  }
  /* The test is over */
  printf("The results:\n"
  "Clients used: %d\n"
  "Servers used: %d\n"
  "Shared epoll: %d\n"
  "Data size   : %lu\n"
  "Amount of pings received in %lu sec: %lu\n"
  "Throughput: %lu b/s, %lu kb/s, %lu mb/s, %lu gb/s\n",
  nclients,
  nservers,
  shared_epoll,
  ping_size * how_many_pings,
  time_to_wait_s,
  msges,
  (msges * ping_size) / time_to_wait_s,
  (msges * ping_size) / (time_to_wait_s * 1000U),
  (msges * ping_size) / (time_to_wait_s * 1000000U),
  (msges * ping_size) / (time_to_wait_s * 1000000000U)
  );
  /* Cleanup for valgrind */
  pthread_mutex_unlock(&mutex);
  pthread_mutex_destroy(&mutex);
  sem_destroy(&sem);
  net_epoll_stop(&socket_epoll);
  net_epoll_free(&socket_epoll);
  if(shared_epoll == 0) {
    net_epoll_stop(&server_epoll);
    net_epoll_free(&server_epoll);
  }
  time_manager_stop(&manager);
  time_manager_free(&manager);
  TEST_PASS;
  return 0;
}