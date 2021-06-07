#include "net.h"

#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <signal.h>
#include <endian.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/ip.h>

struct addrinfo net_get_addr_struct(const int family, const int socktype, const int protocol, const int flags) {
  return (struct addrinfo) {
    .ai_family = family,
    .ai_socktype = socktype,
    .ai_protocol = protocol,
    .ai_flags = flags
  };
}

struct addrinfo* net_get_address(const char* const hostname, const char* const service, const struct addrinfo* const hints) {
  struct addrinfo* addr;
  int err = getaddrinfo(hostname, service, hints, &addr);
  if(err != 0) {
    errno = err;
    return NULL;
  }
  return addr;
}

const char* net_get_address_strerror(const int code) {
  return gai_strerror(code);
}

void net_get_address_free(struct addrinfo* const info) {
  freeaddrinfo(info);
}

int net_get_name(const void* const sockaddr, const socklen_t sockaddrlen, char* const hostname, const socklen_t len, const int flags) {
  return getnameinfo((struct sockaddr*) sockaddr, sockaddrlen, hostname, len, NULL, 0, NI_NAMEREQD | flags);
}

int net_string_to_address(void* const addr, const char* const buffer) {
  int err;
  if(((struct sockaddr*) addr)->sa_family == ipv4) {
    err = inet_pton(ipv4, buffer, &((struct sockaddr_in*) addr)->sin_addr.s_addr);
  } else {
    err = inet_pton(ipv6, buffer, ((struct sockaddr_in6*) addr)->sin6_addr.s6_addr);
  }
  if(err == 1) {
    return net_success;
  }
  return net_failure;
}

int net_address_to_string(void* const addr, char* const buffer) {
  const void* err;
  if(((struct sockaddr*) addr)->sa_family == ipv4) {
    err = inet_ntop(ipv4, &((struct sockaddr_in*) addr)->sin_addr.s_addr, buffer, ipv4_strlen);
  } else {
    err = inet_ntop(ipv6, ((struct sockaddr_in6*) addr)->sin6_addr.s6_addr, buffer, ipv6_strlen);
  }
  if(err == NULL) {
    return net_failure;
  }
  return net_success;
}

void net_set_family(void* const addr, const int family) {
  ((struct sockaddr*) addr)->sa_family = family;
}

int net_get_family(const void* const addr) {
  return ((struct sockaddr*) addr)->sa_family;
}

void net_set_any_addr(void* const addr) {
  if(((struct sockaddr*) addr)->sa_family == ipv4) {
    ((struct sockaddr_in*) addr)->sin_addr.s_addr = INADDR_ANY;
  } else {
    ((struct sockaddr_in6*) addr)->sin6_addr = in6addr_any;
  }
}

void net_set_loopback_addr(void* const addr) {
  if(((struct sockaddr*) addr)->sa_family == ipv4) {
    ((struct sockaddr_in*) addr)->sin_addr.s_addr = INADDR_LOOPBACK;
  } else {
    ((struct sockaddr_in6*) addr)->sin6_addr = in6addr_loopback;
  }
}

void net_set_addr(void* const addr, const void* const address) {
  if(((struct sockaddr*) addr)->sa_family == ipv4) {
    (void) memcpy(&((struct sockaddr_in*) addr)->sin_addr.s_addr, address, 4);
  } else {
    (void) memcpy(((struct sockaddr_in6*) addr)->sin6_addr.s6_addr, address, 16);
  }
}

void* net_get_addr(const void* const addr) {
  if(((struct sockaddr*) addr)->sa_family == ipv4) {
    return &((struct sockaddr_in*) addr)->sin_addr.s_addr;
  } else {
    return ((struct sockaddr_in6*) addr)->sin6_addr.s6_addr;
  }
}

void net_set_port(void* const addr, const unsigned short port) {
  if(((struct sockaddr*) addr)->sa_family == ipv4) {
    ((struct sockaddr_in*) addr)->sin_port = htons(port);
  } else {
    ((struct sockaddr_in6*) addr)->sin6_port = htons(port);
  }
}

unsigned short net_get_port(void* const addr) {
  if(((struct sockaddr*) addr)->sa_family == ipv4) {
    return ntohs(((struct sockaddr_in*) addr)->sin_port);
  } else {
    return ntohs(((struct sockaddr_in6*) addr)->sin6_port);
  }
}

int net_get_ipv4_addrlen() {
  return sizeof(struct sockaddr_in);
}

int net_get_ipv6_addrlen() {
  return sizeof(struct sockaddr_in6);
}

int net_get_addrlen(const void* const addr) {
  if(((struct sockaddr*) addr)->sa_family == ipv4) {
    return net_get_ipv4_addrlen();
  } else {
    return net_get_ipv6_addrlen();
  }
}

int net_get_socket(const struct addrinfo* const addr) {
  return socket(addr->ai_family, addr->ai_socktype, addr->ai_protocol);
}

int net_bind_socket(const int sfd, const void* const addr) {
  return bind(sfd, (struct sockaddr*) addr, net_get_addrlen(addr));
}

int net_connect_socket(const int sfd, const void* const addr) {
  return connect(sfd, (struct sockaddr*) addr, net_get_addrlen(addr));
}

int net_socket_setopt_true(const int sfd, const int level, const int option_name) {
  if(setsockopt(sfd, level, option_name, &(int){1}, sizeof(int)) == 0) {
    return net_success;
  }
  return net_failure;
}

int net_socket_setopt_false(const int sfd, const int level, const int option_name) {
  if(setsockopt(sfd, level, option_name, &(int){0}, sizeof(int)) == 0) {
    return net_success;
  }
  return net_failure;
}

int net_socket_reuse_addr(const int sfd) {
  return net_socket_setopt_true(sfd, SOL_SOCKET, SO_REUSEADDR);
}

int net_socket_dont_reuse_addr(const int sfd) {
  return net_socket_setopt_false(sfd, SOL_SOCKET, SO_REUSEADDR);
}

int net_socket_reuse_port(const int sfd) {
  return net_socket_setopt_true(sfd, SOL_SOCKET, SO_REUSEPORT);
}

int net_socket_dont_reuse_port(const int sfd) {
  return net_socket_setopt_false(sfd, SOL_SOCKET, SO_REUSEPORT);
}

int net_socket_get_family(const int sfd, int* const family) {
  return getsockopt(sfd, SOL_SOCKET, SO_DOMAIN, family, &(socklen_t){sizeof(int)});
}

int net_socket_get_socktype(const int sfd, int* const socktype) {
  return getsockopt(sfd, SOL_SOCKET, SO_TYPE, socktype, &(socklen_t){sizeof(int)});
}

int net_socket_get_protocol(const int sfd, int* const protocol) {
  return getsockopt(sfd, SOL_SOCKET, SO_PROTOCOL, protocol, &(socklen_t){sizeof(int)});
}

int net_socket_dont_block(const int sfd) {
  const int flags = fcntl(sfd, F_GETFL, 0);
  if(flags == -1) {
    return net_failure;
  }
  const int res = fcntl(sfd, F_SETFL, flags | O_NONBLOCK);
  if(res == -1) {
    return net_failure;
  }
  return net_success;
}

int net_socket_block(const int sfd) {
  int flags = fcntl(sfd, F_GETFL, 0);
  if(flags == -1) {
    return net_failure;
  }
  if((flags ^ O_NONBLOCK) != 0) {
    flags ^= O_NONBLOCK;
  }
  const int res = fcntl(sfd, F_SETFL, flags);
  if(res == -1) {
    return net_failure;
  }
  return net_success;
}

int net_socket_base_options(const int sfd) {
  int err = net_socket_dont_block(sfd);
  if(err != net_success) {
    return net_failure;
  }
  err = net_socket_reuse_addr(sfd);
  if(err != net_success) {
    return net_failure;
  }
  err = net_socket_reuse_port(sfd);
  if(err != net_success) {
    return net_failure;
  }
  return net_success;
}

#define epoll ((struct net_epoll*) net_epoll_thread_data)

static void net_epoll_thread(void* net_epoll_thread_data) {
  (void) pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL);
  {
    sigset_t mask;
    (void) sigfillset(&mask);
    (void) pthread_sigmask(SIG_BLOCK, &mask, NULL);
  }
  struct epoll_event events[100];
  while(1) {
    int count = epoll_wait(epoll->sfd, events, 100, -1);
    for(int i = 0; i < count; ++i) {
      (void) pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);
      epoll->on_event(epoll, events[i].events, events[i].data.ptr);
      (void) pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
      pthread_testcancel();
    }
  }
}

#undef epoll

int net_epoll(struct net_epoll* const epoll, void (*on_event)(struct net_epoll*, int, void*)) {
  {
    const int err = threads(&epoll->thread);
    if(err != threads_success) {
      return net_failure;
    }
  }
  const int sfd = epoll_create1(0);
  if(sfd == -1) {
    threads_free(&epoll->thread);
    return net_failure;
  }
  epoll->sfd = sfd;
  epoll->thread.func = net_epoll_thread;
  epoll->thread.data = epoll;
  epoll->on_event = on_event;
  return net_success;
}

int net_epoll_start(struct net_epoll* const epoll, const unsigned long amount) {
  return threads_add(&epoll->thread, amount);
}

void net_epoll_stop(struct net_epoll* const epoll) {
  threads_shutdown(&epoll->thread);
}

void net_epoll_free(struct net_epoll* const epoll) {
  threads_free(&epoll->thread);
  (void) close(epoll->sfd);
}

static int net_epoll_modify(struct net_epoll* const epoll, struct net_socket_base* const socket, const int method) {
  const int err = epoll_ctl(epoll->sfd, method, socket->sfd, &((struct epoll_event) {
    .events = socket->events,
    .data = (epoll_data_t) {
      .ptr = socket
    }
  }));
  if(err != 0) {
    return net_failure;
  }
  return net_success;
}

int net_epoll_add(struct net_epoll* const epoll, struct net_socket_base* const socket) {
  return net_epoll_modify(epoll, socket, EPOLL_CTL_ADD);
}

int net_epoll_mod(struct net_epoll* const epoll, struct net_socket_base* const socket) {
  return net_epoll_modify(epoll, socket, EPOLL_CTL_MOD);
}

int net_epoll_remove(struct net_epoll* const epoll, struct net_socket_base* const socket) {
  return net_epoll_modify(epoll, socket, EPOLL_CTL_DEL);
}