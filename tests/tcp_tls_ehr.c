#include "tests.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <shnet/tcp.h>
#include <shnet/time.h>

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

void onopen(struct tcp_socket* socket) {
  printf_debug("onopen() socket sfd %d", 1, socket->base.sfd);
  const char* buf = \
  "GET / HTTP/1.1\r\n"
  "Connection: Close\r\n"
  "Host: wikipedia.com\r\n"
  "Origin: http://wikipedia.com\r\n"
  "User-Agent: Mozilla/5.0 (X11; Ubuntu; Linux x86_64; rv:87.0) Gecko/20100101 Firefox/87.0\r\n"
  "Accept-Encoding: gzip\r\n"
  "\r\n";
  int sent = tcp_handler_send(socket, buf, strlen(buf));
  if(sent >= 0) {
    printf_debug("onopen() socket sfd %d successfully sent %d bytes", 1, socket->base.sfd, sent);
  } else if(sent == -1) {
    printf_debug("onopen() socket sfd %d sending error", 1, socket->base.sfd);
  } else {
    printf_debug("onopen() socket sfd %d sending fatal error", 1, socket->base.sfd);
  }
}

void onmessage(struct tcp_socket* socket) {
  printf_debug("onmessage() socket sfd %d", 1, socket->base.sfd);
  unsigned char buf[65535];
  int read = 0;
  int status = tcp_handler_read(socket, buf, 65535, &read);
  if(read != 0) {
    printf_debug("onmessage() socket sfd %d successfully read %d bytes", 1, socket->base.sfd, read);
    buf[read] = 0;
    printf("%s\n", (char*) buf);
  }
  switch(status) {
    case -2: {
      printf_debug("onmessage() socket sfd %d reading fatal error", 1, socket->base.sfd);
      return;
    }
    case -1: {
      printf_debug("onmessage() socket sfd %d reading error", 1, socket->base.sfd);
      return;
    }
    case 0: {
      printf_debug("onmessage() socket sfd %d received FIN, sending FIN back", 1, socket->base.sfd);
      tcp_socket_close(socket);
      break;
    }
  }
}

void onclose(struct tcp_socket* socket) {
  printf_debug("onclose() socket sfd %d reason %d errno %d", 1, socket->base.sfd, socket->close_reason, errno);
  tcp_socket_confirm_close(socket);
  net_epoll_stop(socket->epoll);
  pthread_mutex_unlock(&mutex);
}

int onerror(struct tcp_socket* socket) {
  printf_debug("onerror() socket sfd %d error %d", 1, socket->base.sfd, errno);
  return tcp_proceed;
}

int main() {
  printf_debug("Testing tcp_tls_ehr.c:", 1);
  pthread_mutex_lock(&mutex);
  
  /* Getting an address to connect to */
  char ip[ip_max_strlen];
  struct addrinfo hints = net_get_addr_struct(ipv4, stream_socktype, tcp_protocol, numeric_service);
  struct addrinfo* res = net_get_address("wikipedia.com", "443", &hints);
  if(res == NULL) {
    printf("err %s\n", net_get_address_strerror(errno));
    TEST_FAIL;
  }
  
  /* TCP setup */
  struct net_epoll epoll;
  int err = net_epoll(&epoll, tcp_onevent);
  if(err == net_failure) {
    TEST_FAIL;
  }
  net_epoll_start(&epoll, 1);
  
  struct tcp_socket socket;
  memset(&socket, 0, sizeof(socket));
  socket.epoll = &epoll;
  
  struct tcp_socket_callbacks socket_callbacks;
  socket_callbacks.onopen = onopen;
  socket_callbacks.onmessage = onmessage;
  socket_callbacks.onclose = onclose;
  socket_callbacks.onerror = onerror;
  socket.callbacks = &socket_callbacks;
  
  struct tcp_socket_settings socket_settings;
  socket_settings.send_buffer_cleanup_threshold = 1000000;
  socket_settings.send_buffer_allow_freeing = 1;
  socket.settings = &socket_settings;
  
  SSL_library_init();
  SSL_CTX* ctx = SSL_CTX_new(TLS_client_method());
  if(ctx == NULL) {
    TEST_FAIL;
  }
  (void) SSL_CTX_set_min_proto_version(ctx, TLS1_2_VERSION);
  (void) SSL_CTX_set_mode(ctx, SSL_MODE_ENABLE_PARTIAL_WRITE);
  socket.ctx = ctx;
  
  /* Picking an address & creating a connection */
  struct addrinfo* info = res;
  do {
    err = net_address_to_string(info->ai_addr, ip);
    if(err == net_failure) {
      TEST_FAIL;
    }
    memcpy(&socket.base.addr, info->ai_addr, sizeof(struct sockaddr_in));
    err = tcp_create_socket(&socket);
    if(err == net_failure) {
      printf("err %s\n", net_get_address_strerror(errno));
      TEST_FAIL;
    }
    printf_debug("Connecting socket sfd %d at %s", 1, socket.base.sfd, ip);
    break;
    info = info->ai_next;
  } while(info != NULL);
  net_get_address_free(res);
  if(info == NULL) {
    TEST_FAIL;
  }
  info = NULL;
  res = NULL;
  pthread_mutex_lock(&mutex);
  pthread_mutex_unlock(&mutex);
  pthread_mutex_destroy(&mutex);
  net_epoll_free(socket.epoll);
  TEST_PASS;
  return 0;
}