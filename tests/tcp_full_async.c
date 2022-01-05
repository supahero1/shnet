#include "tests.h"

#include <errno.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <inttypes.h>

#include <shnet/tcp.h>
#include <shnet/error.h>

_Atomic uint64_t total = 0;
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

int socket_onevent(struct tcp_socket* socket, enum tcp_event event) {
  switch(event) {
    case tcp_free: {
      (void) pthread_mutex_unlock(&mutex);
      break;
    }
    default: break;
  }
  return 0;
}

int serversock_onevent(struct tcp_socket* socket, enum tcp_event event) {
  switch(event) {
    case tcp_data: {
      errno = 0;
      char buf[512];
      uint64_t read = 0;
      uint64_t r;
      do {
        r = tcp_read(socket, buf, 512);
        read += r;
      } while(r != 0 && errno == 0);
      atomic_fetch_add(&total, read);
      if(read != 0 && atomic_load(&total) == 17) {
        tcp_socket_close(socket);
        tcp_socket_free(socket); /* An example of async usage too */
      }
      break;
    }
    /*case tcp_close: {                               ^
      tcp_socket_free(socket);                        |
      break;                                          |
    }*/
    default: break;
  }
  return 0;
}

int server_onevent(struct tcp_server* server, enum tcp_event event, struct tcp_socket* socket, const struct sockaddr* addr) {
  switch(event) {
    case tcp_creation: {
      socket->on_event = serversock_onevent;
      break;
    }
    case tcp_close: {
      tcp_server_free(server);
      break;
    }
    case tcp_free: {
      (void) pthread_mutex_unlock(&mutex);
      break;
    }
    default: break;
  }
  return 0;
}

int onerr(int code) {
  return -1;
}

int main() {
  _debug("Testing tcp_full_async:", 1);
  handle_error = onerr;
  pthread_mutex_lock(&mutex);
  struct tcp_server_settings sets = { 1, 1 };
  
  _debug("1. Initialization", 1);
  _debug("1.1.", 1);
  /* Setup a server first */
  struct tcp_server server = {0};
  server.on_event = server_onevent;
  server.settings = &sets;
  if(tcp_server(&server, &((struct tcp_server_options) {
    .hostname = "localhost",
    .port = "8765",
    .family = ipv4,
    .flags = 0
  })) != 0) {
    _debug("tcp_server() errno %d", 1, errno);
    TEST_FAIL;
  }
  TEST_PASS;
  
  _debug("1.2.", 1);
  /* Now the fully asynchronous socket */
  struct tcp_socket socket = {0};
  socket.on_event = socket_onevent;
  if(tcp_socket(&socket, &((struct tcp_socket_options) {
    .hostname = "localhost",
    .port = "8765",
    .family = ipv4,
    .flags = 0,
    .static_hostname = 1,
    .static_port = 1
  })) != 0) {
    _debug("tcp_socket() errno %d", 1, errno);
    TEST_FAIL;
  }
  TEST_PASS;
  
  _debug("2. Test the feature", 1);
  /* Send some data, free after close. Run with Valgrind to double check no mem
  leaks. It sure is very tempting to also make tcp_socket_close() asynchronous,
  but that would require a lot more logic to implement. */
  if(tcp_send(&socket, &(uint8_t[]){ 1, 3, 2, 4, 3, 5, 4, 6, 5, 7, 6, 8, 7, 9, 8, 0, 9 }, 17, data_dont_free) != 0) { // TODO tcp_socket_close IS async, but only if we dont make the underlying code make a dns query!!!
    _debug("tcp_send() errno %d", 1, errno);
    TEST_FAIL;
  }
  tcp_socket_free(&socket);
  pthread_mutex_lock(&mutex);
  if(tcp_server_shutdown(&server)) {
    _debug("tcp_server_shutdown() errno %d", 1, errno);
    TEST_FAIL;
  }
  pthread_mutex_lock(&mutex);
  usleep(1000); /* To make sure thread of the async socket completed its shutdown */
  TEST_PASS;
  
  _debug("Testing tcp_full_async succeeded", 1);
  debug_free();
  return 0;
}