#include "tests.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <shnet/udp.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

int socket_sfd;
int server_sfd;

const unsigned long amount = 1024;

pthread_mutex_t lock;

#define socket ((struct udp_socket*) ptr)

void on_event(struct net_epoll* epoll, int event, struct net_socket_base* ptr) {
  uint8_t recv_buf[65535];
  if(socket->base.sfd == socket_sfd) {
    if((event & EPOLLOUT) != 0) {
      pthread_mutex_unlock(&lock);
    }
    if((event & EPOLLIN) != 0) {
      ssize_t bytes = recv(socket->base.sfd, recv_buf, 65535, 0);
      if(bytes != amount) {
        TEST_FAIL;
      }
      pthread_mutex_unlock(&lock);
    }
  } else {
    if((event & EPOLLIN) != 0) {
      struct sockaddr_in6 server_addr;
      socklen_t server_len = sizeof(server_addr);
      ssize_t bytes = recvfrom(socket->base.sfd, recv_buf, 65535, 0, (struct sockaddr*)&server_addr, &server_len);
      if(bytes != amount) {
        TEST_FAIL;
      }
      char ip[ip_max_strlen];
      if(net_address_to_string(&server_addr, ip) == net_failure) {
        TEST_FAIL;
      }
      bytes = sendto(socket->base.sfd, recv_buf, amount, MSG_CONFIRM, (struct sockaddr*)&server_addr, server_len);
      if(bytes != amount) {
        TEST_FAIL;
      }
    }
  }
}

#undef socket

#define check_err \
do { \
  if(err != net_success) { \
    printf("\nError, errno %d\n", errno); \
    TEST_FAIL; \
  } \
} while(0)

int main() {
  printf_debug("Testing udp.c:", 1);
  puts("Please keep the port 8099 empty for the test to succeed.");
  int err;
  
  err = pthread_mutex_init(&lock, NULL);
  if(err != 0) {
    TEST_FAIL;
  }
  pthread_mutex_lock(&lock);
  
  
  
  struct net_epoll epoll;
  memset(&epoll, 0, sizeof(epoll));
  err = net_epoll(&epoll, on_event, net_epoll_no_wakeup_method);
  check_err;
  err = net_epoll_start(&epoll, 1);
  check_err;
  
  
  
  struct udp_socket server;
  server.base.events = EPOLLIN | EPOLLOUT | EPOLLET;
  net_set_family(&server.base.addr, ipv4);
  net_set_port(&server.base.addr, 8099);
  net_set_any_addr(&server.base.addr);
  err = udp_create_server(&server);
  check_err;
  err = net_epoll_add(&epoll, &server.base);
  check_err;
  server_sfd = server.base.sfd;
  
  
  
  struct udp_socket socket;
  socket.base.events = EPOLLIN | EPOLLOUT | EPOLLET;
  net_set_family(&socket.base.addr, ipv4);
  net_set_port(&socket.base.addr, 8091);
  net_set_any_addr(&socket.base.addr);
  err = udp_create_socket(&socket);
  check_err;
  err = net_epoll_add(&epoll, &socket.base);
  check_err;
  socket_sfd = socket.base.sfd;
  
  
  pthread_mutex_lock(&lock);
  
  
  uint8_t* trash = malloc(amount);
  if(trash == NULL) {
    TEST_FAIL;
  }
  
  ssize_t code = send(socket.base.sfd, trash, amount, 0);
  if(code != amount) {
    TEST_FAIL;
  }
  
  pthread_mutex_lock(&lock);
  puts("3");
  
  err = net_epoll_remove(&epoll, &socket.base);
  check_err;
  err = net_epoll_remove(&epoll, &server.base);
  check_err;
  net_epoll_stop(&epoll);
  net_epoll_free(&epoll);
  udp_close(&server);
  udp_close(&socket);
  free(trash);
  TEST_PASS;
  return 0;
}