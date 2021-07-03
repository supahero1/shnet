#include "tests.h"

#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <semaphore.h>
#include <shnet/tls.h>
#include <shnet/time.h>
#include <openssl/ssl.h>

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
char buf[32000000];

#define DESTINATION "static.diep.io"

#define socket ((struct tls_socket*) data)

int socket_pick_address(struct addrinfo* info, void* data) {
  char ip[ip_max_strlen];
  net_sockbase_set_family(&socket->base, net_addrinfo_get_family(info));
  net_sockbase_set_whole_addr(&socket->base, net_addrinfo_get_whole_addr(info));
  (void) net_address_to_string(net_sockbase_get_whole_addr(&socket->base), ip);
  if(tls_create_socket(socket) != net_success) {
    printf("socket_pick_address() err %s errno %d\n", net_strerror(errno), errno);
    return net_failure;
  } else {
    printf("connected at %s\n", ip);
    return net_success;
  }
}

#undef socket

void onopen(struct tls_socket* socket) {
  puts("socket opened!");
  pthread_mutex_unlock(&mutex);
}

void onmessage(struct tls_socket* socket) {
  puts("onmessage");
  int read = tls_read(socket, buf, 32000000);
  printf("read %d bytes:\n%s\n", read, buf);
  //pthread_mutex_unlock(&mutex);
}

int onnomem(struct tls_socket* socket) {
  puts("no mem");
  exit(0);
}

void onclose(struct tls_socket* socket) {
  puts("socket closed rip");
}

int main(int argc, char **argv) {
  puts("Testing tls.c:");
  pthread_mutex_lock(&mutex);
  
  SSL_library_init();
  
  struct net_epoll epoll;
  struct tls_socket socket;
  memset(&epoll, 0, sizeof(epoll));
  memset(&socket, 0, sizeof(socket));
  struct tcp_socket_callbacks sock_tcp_cb;
  struct tcp_socket_settings sock_tcp_set;
  struct tls_socket_callbacks sock_tls_cb;
  struct tls_socket_settings sock_tls_set;
  
  int err = tcp_epoll(&epoll);
  if(err == net_failure) {
    TEST_FAIL;
  }
  if(net_epoll_start(&epoll, 1) != net_success) {
    TEST_FAIL;
  }
  
  memset(buf, 0, sizeof(buf));
  
  sock_tcp_cb = tls_default_tcp_socket_callbacks;
  
  sock_tcp_set = (struct tcp_socket_settings) {
    .send_buffer_cleanup_threshold = 0,
    .send_buffer_allow_freeing = 1,
    .disable_send_buffer = 0,
    .onreadclose_auto_res = 1,
    .remove_from_epoll_onclose = 1
  };
  
  sock_tls_cb = (struct tls_socket_callbacks) {
    .onopen = onopen,
    .onmessage = onmessage,
    .onclose = onclose,
    .onnomem = onnomem,
    .tcp_onreadclose = NULL,
    .tls_onreadclose = NULL
  };
  
  sock_tls_set = (struct tls_socket_settings) {
    .read_buffer_cleanup_threshold = 0,
    .read_buffer_growth = 4096,
    .read_buffer_allow_freeing = 1,
    .force_close_on_fatal_error = 0,
    .force_close_on_shutdown_error = 0,
    .force_close_tcp = 0,
    .onreadclose_auto_res = tls_onreadclose_tls_close
  };
  
  SSL_CTX* ctx;
  ctx = SSL_CTX_new(TLS_client_method());
  if(ctx == NULL) {
    TEST_FAIL;
  }
  (void) SSL_CTX_set_min_proto_version(ctx, TLS1_2_VERSION);
  
  socket.callbacks = &sock_tcp_cb;
  socket.settings = &sock_tcp_set;
  socket.tls_callbacks = &sock_tls_cb;
  socket.tls_settings = &sock_tls_set;
  socket.epoll = &epoll;
  socket.ctx = ctx;
  
  struct addrinfo hints = net_get_addr_struct(any_family, stream_socktype, tcp_protocol, numeric_service | wants_own_ip_version);
  struct addrinfo* res = net_get_address(DESTINATION, "443", &hints);
  if(res == NULL) {
    printf("err %s\n", net_strerror(errno));
    TEST_FAIL;
  }
  
  if(net_foreach_addrinfo(res, socket_pick_address, &socket) == net_failure) {
    TEST_FAIL;
  }
  net_get_address_free(res);
  
  (void) SSL_set_tlsext_host_name(socket.ssl, DESTINATION);
  
  pthread_mutex_lock(&mutex);
  char request[] = ""
  "GET /d.js HTTP/1.1\r\n"
  "Connection: close\r\n"
  "Accept: */*\r\n"
  "Accept-Language: en-US,en;q=0.9\r\n"
  "Host: " DESTINATION "\r\n"
  "Origin: https://" DESTINATION "\r\n"
  "Transfer-Encoding: identity\r\n"
  "User-Agent: Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/91.0.4472.114 Safari/537.36\r\n"
  "\r\n";
  tls_send(&socket, request, sizeof(request));
  printf("sent %ld bytes\n", sizeof(request));
  pthread_mutex_lock(&mutex);
  TEST_PASS;
  return 0;
}