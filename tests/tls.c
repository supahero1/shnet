#include "tests.h"

#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <semaphore.h>
#include <shnet/tls.h>
#include <shnet/time.h>

sem_t sem;
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
unsigned long msges = 0;

#define how_many_pings 1000UL
#define ping_size 32UL
#define time_to_wait_s 1UL

struct tcp_socket_callbacks serversock_tcp_cb;
struct tcp_socket_settings serversock_tcp_set;

struct tls_socket_callbacks serversock_tls_cb;
struct tls_socket_settings serversock_tls_set;

struct tcp_socket_callbacks clientsock_tcp_cb;
struct tcp_socket_settings clientsock_tcp_set;

struct tls_socket_callbacks clientsock_tls_cb;
struct tls_socket_settings clientsock_tls_set;

struct tcp_server_callbacks server_tcp_cb;
struct tls_server_callbacks server_tls_cb;
struct tcp_server_settings server_set;

void serversock_onopen(struct tls_socket* socket) {
  (void) socket;
  sem_post(&sem);
}

void serversock_onmessage(struct tls_socket* socket) {
  unsigned char buf[ping_size * how_many_pings];
  errno = 0;
  while(errno == 0) {
    int read = tls_read(socket, buf, ping_size * how_many_pings);
    if(read == 0) {
      return;
    }
    const int pings = read / ping_size;
    msges += pings;
    (void) tls_send(socket, buf, ping_size * pings);
  }
}

void serversock_onclose(struct tls_socket* socket) {
  sem_post(&sem);
  tls_socket_free(socket);
}



void onopen(struct tls_socket* socket) {
  (void) socket;
  sem_post(&sem);
}

void onmessage(struct tls_socket* socket) {
  unsigned char buf[ping_size * how_many_pings];
  errno = 0;
  while(errno == 0) {
    int read = tls_read(socket, buf, ping_size * how_many_pings);
    if(read == 0) {
      return;
    }
    const int pings = read / ping_size;
    (void) tls_send(socket, buf, ping_size * pings);
  }
}

void onclose(struct tls_socket* socket) {
  sem_post(&sem);
  tls_socket_free(socket);
}

int onconnection(struct tls_socket* socket) {
  socket->callbacks = &serversock_tcp_cb;
  socket->settings = &serversock_tcp_set;
  socket->tls_callbacks = &serversock_tls_cb;
  socket->tls_settings = &serversock_tls_set;
  if(tls_socket_init(socket, net_server) != 0) {
    TEST_FAIL;
  }
  return 0;
}

int sock_onnomem(struct tls_socket* socket) {
  TEST_FAIL;
  return -1;
}

int serv_onnomem(struct tls_server* server) {
  TEST_FAIL;
  return -1;
}

void onshutdown(struct tls_server* server) {
  tls_server_free(server);
  sem_post(&sem);
}

#define server ((struct tls_server*) data)

int server_pick_address(struct addrinfo* info, void* data) {
  char ip[ip_max_strlen];
  net_sockbase_set_family(&server->base, net_addrinfo_get_family(info));
  net_sockbase_set_whole_addr(&server->base, net_addrinfo_get_whole_addr(info));
  (void) net_address_to_string(net_sockbase_get_whole_addr(&server->base), ip);
  if(tls_create_server(server) != 0) {
    printf("server_pick_address() err %s errno %d\n", net_strerror(errno), errno);
    return -1;
  } else {
    return 0;
  }
}

#undef server
#define socket ((struct tls_socket*) data)

int socket_pick_address(struct addrinfo* info, void* data) {
  char ip[ip_max_strlen];
  net_sockbase_set_family(&socket->base, net_addrinfo_get_family(info));
  net_sockbase_set_whole_addr(&socket->base, net_addrinfo_get_whole_addr(info));
  (void) net_address_to_string(net_sockbase_get_whole_addr(&socket->base), ip);
  if(tls_create_socket(socket) != 0) {
    printf("socket_pick_address() err %s errno %d\n", net_strerror(errno), errno);
    return -1;
  } else {
    return 0;
  }
}

#undef socket

void unlock_mutexo(void* da) {
  (void) da;
  pthread_mutex_unlock(&mutex);
}

int main(int argc, char **argv) {
  puts("Testing tls:");
  srand(time_get_time());
  sem_init(&sem, 0, 0);
  pthread_mutex_lock(&mutex);
  tls_ignore_sigpipe();
  
  memset(&serversock_tcp_cb, 0, sizeof(serversock_tcp_cb));
  memset(&serversock_tcp_set, 0, sizeof(serversock_tcp_set));
  memset(&serversock_tls_cb, 0, sizeof(serversock_tls_cb));
  memset(&serversock_tls_set, 0, sizeof(serversock_tls_set));
  memset(&clientsock_tcp_cb, 0, sizeof(clientsock_tcp_cb));
  memset(&clientsock_tcp_set, 0, sizeof(clientsock_tcp_set));
  memset(&clientsock_tls_cb, 0, sizeof(clientsock_tls_cb));
  memset(&clientsock_tls_set, 0, sizeof(clientsock_tls_set));
  memset(&server_tcp_cb, 0, sizeof(server_tcp_cb));
  memset(&server_tls_cb, 0, sizeof(server_tls_cb));
  memset(&server_set, 0, sizeof(server_set));
  
  serversock_tcp_cb = tls_default_tcp_socket_callbacks;
  
  serversock_tcp_set = (struct tcp_socket_settings) {
    .send_buffer_cleanup_threshold = 0,
    .onreadclose_auto_res = 1,
    .remove_from_epoll_onclose = 0
  };
  
  serversock_tls_cb = (struct tls_socket_callbacks) {
    .oncreation = NULL,
    .onopen = serversock_onopen,
    .onmessage = serversock_onmessage,
    .tcp_onreadclose = NULL,
    .tls_onreadclose = NULL,
    .onnomem = sock_onnomem,
    .onclose = serversock_onclose,
    .onfree = NULL
  };
  
  serversock_tls_set = (struct tls_socket_settings) {
    .read_buffer_cleanup_threshold = 0,
    .read_buffer_growth = 4096,
    .force_close_on_fatal_error = 1,
    .force_close_on_shutdown_error = 1,
    .force_close_tcp = 1,
    .onreadclose_auto_res = tls_onreadclose_tls_close
  };
  
  clientsock_tcp_cb = serversock_tcp_cb;
  
  clientsock_tcp_set = serversock_tcp_set;
  
  clientsock_tls_cb = (struct tls_socket_callbacks) {
    .oncreation = NULL,
    .onopen = onopen,
    .onmessage = onmessage,
    .tcp_onreadclose = NULL,
    .tls_onreadclose = NULL,
    .onnomem = sock_onnomem,
    .onclose = onclose,
    .onfree = NULL
  };
  
  clientsock_tls_set = serversock_tls_set;
  
  server_tcp_cb = tls_default_tcp_server_callbacks;
  
  server_tls_cb = (struct tls_server_callbacks) {
    .onconnection = onconnection,
    .onnomem = serv_onnomem,
    .onshutdown = onshutdown
  };
  
  server_set.max_conn = 10000;
  server_set.backlog = 128;
  
  const int nclients = atoi(argv[1]);
  const int nservers = atoi(argv[2]);
  const int shared_epoll = atoi(argv[3]);
  
  /* Epoll setup */
  struct net_epoll socket_epoll;
  memset(&socket_epoll, 0, sizeof(socket_epoll));
  if(tls_epoll(&socket_epoll) != 0) {
    TEST_FAIL;
  }
  if(net_epoll_start(&socket_epoll, 1) != 0) {
    TEST_FAIL;
  }
  
  struct net_epoll server_epoll;
  if(shared_epoll == 0) {
    memset(&server_epoll, 0, sizeof(server_epoll));
    if(tls_epoll(&server_epoll) != 0) {
      TEST_FAIL;
    }
    if(net_epoll_start(&server_epoll, 1) != 0) {
      TEST_FAIL;
    }
  }
  
  SSL_CTX* client_ctx = SSL_CTX_new(TLS_client_method());
  if(client_ctx == NULL) {
    TEST_FAIL;
  }
  
  SSL_CTX* server_ctx = SSL_CTX_new(TLS_server_method());
  if(server_ctx == NULL) {
    TEST_FAIL;
  }
  
  char cert_dest[512];
  memset(cert_dest, 0, sizeof(cert_dest));
  if(getcwd(cert_dest, 490) == NULL) {
    TEST_FAIL;
  }
  strcat(cert_dest, "/tests/cert.pem");
  if(SSL_CTX_use_certificate_file(server_ctx, cert_dest, SSL_FILETYPE_PEM) != 1) {
    TEST_FAIL;
  }
  char key_dest[512];
  memset(key_dest, 0, sizeof(key_dest));
  if(getcwd(key_dest, 490) == NULL) {
    TEST_FAIL;
  }
  strcat(key_dest, "/tests/key.pem");
  if(SSL_CTX_use_PrivateKey_file(server_ctx, key_dest, SSL_FILETYPE_PEM) != 1) {
    TEST_FAIL;
  }
  
  
  
  struct addrinfo hints = net_get_addr_struct(any_family, stream_socktype, tcp_protocol, numeric_service | wants_a_server | wants_own_ip_version);
  struct addrinfo* res = net_get_address(NULL, "8099", &hints);
  if(res == NULL) {
    printf("err %s\n", net_strerror(errno));
    TEST_FAIL;
  }
  
  /* TLS server setup */
  struct tls_server* servers = malloc(sizeof(struct tls_server) * nservers);
  if(servers == NULL) {
    TEST_FAIL;
  }
  memset(servers, 0, sizeof(struct tls_server) * nservers);
  for(int i = 0; i < nservers; ++i) {
    if(shared_epoll == 1) {
      servers[i].epoll = &socket_epoll;
    } else {
      servers[i].epoll = &server_epoll;
    }
    servers[i].settings = &server_set;
    servers[i].callbacks = &server_tcp_cb;
    servers[i].tls_callbacks = &server_tls_cb;
    servers[i].ctx = server_ctx;
    if(net_foreach_addrinfo(res, server_pick_address, &servers[i]) != 0) {
      TEST_FAIL;
    }
  }
  net_get_address_free(res);
  
  /* TLS socket setup */
  
  hints = net_get_addr_struct(any_family, stream_socktype, tcp_protocol, numeric_service | numeric_hostname | wants_own_ip_version);
  res = net_get_address("0.0.0.0", "8099", &hints);
  if(res == NULL) {
    printf("err %s\n", net_strerror(errno));
    TEST_FAIL;
  }
  
  
  
  struct tls_socket* sockets = malloc(sizeof(struct tls_socket) * nclients);
  if(servers == NULL) {
    TEST_FAIL;
  }
  memset(sockets, 0, sizeof(struct tls_socket) * nclients);
  for(int i = 0; i < nclients; ++i) {
    sockets[i].epoll = &socket_epoll;
    sockets[i].callbacks = &clientsock_tcp_cb;
    sockets[i].settings = &clientsock_tcp_set;
    sockets[i].tls_callbacks = &clientsock_tls_cb;
    sockets[i].tls_settings = &clientsock_tls_set;
    sockets[i].ctx = client_ctx;
    
    /* Pick an address for the socket */
    if(net_foreach_addrinfo(res, socket_pick_address, &sockets[i]) != 0) {
      TEST_FAIL;
    }
  }
  net_get_address_free(res);
  
  /* Time setup to stop the test */
  struct time_manager manager;
  memset(&manager, 0, sizeof(manager));
  if(time_manager(&manager) != 0) {
    TEST_FAIL;
  }
  if(time_manager_start(&manager) != 0) {
    TEST_FAIL;
  }
  for(int i = 0; i < (nclients << 1); ++i) {
    sem_wait(&sem);
  }
  if(time_manager_add_timeout(&manager, time_get_sec(time_to_wait_s), unlock_mutexo, NULL, NULL) != 0) {
    TEST_FAIL;
  }
  unsigned char buf[ping_size * how_many_pings];
  for(unsigned long i = 0; i < ping_size * how_many_pings; ++i) {
    buf[i] = rand();
  }
  for(int i = 0; i < nclients; ++i) {
    errno = 0;
    (void) tls_send(&sockets[i], buf, ping_size * how_many_pings);
  }
  /* Wait for the timeout to expire, we will be sending data in the background */
  pthread_mutex_lock(&mutex);
  /* Close the clients */
  for(int i = 0; i < nclients; ++i) {
    tls_socket_force_close(&sockets[i]);
  }
  for(int i = 0; i < (nclients << 1); ++i) {
    sem_wait(&sem);
  }
  /* Close the servers */
  for(int i = 0; i < nservers; ++i) {
    if(tls_server_shutdown(&servers[i]) != 0) {
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
  /* Cleanup for valgrind deleted, look tls_stress.c */
  TEST_PASS;
  return 0;
}