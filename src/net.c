#include "net.h"
#include "debug.h"

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
#include <sys/eventfd.h>

struct addrinfo net_get_addr_struct(const int family, const int socktype, const int protocol, const int flags) {
  return (struct addrinfo) {
    .ai_family = family,
    .ai_socktype = socktype,
    .ai_protocol = protocol,
    .ai_flags = flags
  };
}

struct addrinfo* net_get_address(const char* const hostname, const char* const port, const struct addrinfo* const hints) {
  struct addrinfo* addr;
  int err = getaddrinfo(hostname, port, hints, &addr);
  if(err != 0) {
    errno = err;
    return NULL;
  }
  return addr;
}

#define addr ((struct net_async_address*) net_get_address_thread_data)

static void* net_get_address_thread(void* net_get_address_thread_data) {
  addr->callback(addr, net_get_address(addr->hostname, addr->port, addr->hints));
  (void) pthread_detach(pthread_self());
  return NULL;
}

#undef addr

int net_get_address_async(struct net_async_address* const addr) {
  pthread_t id;
  const int err = pthread_create(&id, NULL, net_get_address_thread, addr);
  if(err != 0) {
    errno = err;
    return -1;
  }
  return 0;
}

void net_free_address(struct addrinfo* const info) {
  freeaddrinfo(info);
}

int net_address_to_string(void* const addr, char* const buffer) {
  const void* err;
  if(((struct sockaddr*) addr)->sa_family == ipv4) {
    err = inet_ntop(ipv4, &((struct sockaddr_in*) addr)->sin_addr.s_addr, buffer, ipv4_strlen);
  } else {
    err = inet_ntop(ipv6, ((struct sockaddr_in6*) addr)->sin6_addr.s6_addr, buffer, ipv6_strlen);
  }
  if(err == NULL) {
    return -1;
  }
  return 0;
}

int net_get_ipv4_addrlen() {
  return sizeof(struct sockaddr_in);
}

int net_get_ipv6_addrlen() {
  return sizeof(struct sockaddr_in6);
}

int net_socket_get(const struct addrinfo* const info) {
  return socket(info->ai_family, info->ai_socktype, info->ai_protocol);
}

#define socket ((struct net_socket*) sock)

int net_socket_bind(const void* const sock, const struct addrinfo* const info) {
  return bind(socket->sfd, info->ai_addr, info->ai_family == ipv4 ? net_get_ipv4_addrlen() : net_get_ipv6_addrlen());
}

int net_socket_connect(const void* const sock, const struct addrinfo* const info) {
  return connect(socket->sfd, info->ai_addr, info->ai_family == ipv4 ? net_get_ipv4_addrlen() : net_get_ipv6_addrlen());
}

int net_socket_setopt_true(const void* const sock, const int level, const int option_name) {
  if(setsockopt(socket->sfd, level, option_name, &(int){1}, sizeof(int)) == 0) {
    return 0;
  }
  return -1;
}

int net_socket_setopt_false(const void* const sock, const int level, const int option_name) {
  if(setsockopt(socket->sfd, level, option_name, &(int){0}, sizeof(int)) == 0) {
    return 0;
  }
  return -1;
}

int net_socket_reuse_addr(const void* const sock) {
  return net_socket_setopt_true(sock, SOL_SOCKET, SO_REUSEADDR);
}

int net_socket_dont_reuse_addr(const void* const sock) {
  return net_socket_setopt_false(sock, SOL_SOCKET, SO_REUSEADDR);
}

int net_socket_reuse_port(const void* const sock) {
  return net_socket_setopt_true(sock, SOL_SOCKET, SO_REUSEPORT);
}

int net_socket_dont_reuse_port(const void* const sock) {
  return net_socket_setopt_false(sock, SOL_SOCKET, SO_REUSEPORT);
}

int net_socket_get_family(const void* const sock, int* const family) {
  return getsockopt(socket->sfd, SOL_SOCKET, SO_DOMAIN, family, &(socklen_t){sizeof(int)});
}

int net_socket_get_socktype(const void* const sock, int* const socktype) {
  return getsockopt(socket->sfd, SOL_SOCKET, SO_TYPE, socktype, &(socklen_t){sizeof(int)});
}

int net_socket_get_protocol(const void* const sock, int* const protocol) {
  return getsockopt(socket->sfd, SOL_SOCKET, SO_PROTOCOL, protocol, &(socklen_t){sizeof(int)});
}

int net_socket_get_address(const void* const sock, void* const address) {
  return getpeername(socket->sfd, address, &(socklen_t){sizeof(struct sockaddr_in6)});
}

int net_socket_dont_block(const void* const sock) {
  const int flags = fcntl(socket->sfd, F_GETFL, 0);
  if(flags == -1) {
    return -1;
  }
  const int res = fcntl(socket->sfd, F_SETFL, flags | O_NONBLOCK);
  if(res == -1) {
    return -1;
  }
  return 0;
}

int net_socket_block(const void* const sock) {
  const int flags = fcntl(socket->sfd, F_GETFL, 0);
  if(flags == -1) {
    return -1;
  }
  const int res = fcntl(socket->sfd, F_SETFL, flags & ~O_NONBLOCK);
  if(res == -1) {
    return -1;
  }
  return 0;
}

int net_socket_default_options(const void* const sock) {
  if(net_socket_dont_block(sock) != 0 ||
     net_socket_reuse_addr(sock) != 0 ||
     net_socket_reuse_port(sock) != 0) {
    return -1;
  }
  return 0;
}

#undef socket

#define epoll ((struct net_epoll*) net_epoll_thread_data)
#define event_net ((struct net_socket*) events[i].data.ptr)

static void* net_epoll_thread(void* net_epoll_thread_data) {
  (void) pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL);
  struct epoll_event events[NET_EPOLL_DEFAULT_MAX_EVENTS];
  while(1) {
    int count = epoll_wait(epoll->fd, events, NET_EPOLL_DEFAULT_MAX_EVENTS, -1);
    int check = 0;
    for(int i = 0; i < count; ++i) {
      if(event_net->wakeup) {
        uint64_t r;
        const int ret = eventfd_read(epoll->net.sfd, &r);
        (void) ret;
        check = 1;
        continue;
      }
      epoll->on_event(epoll, events[i].events, event_net);
    }
    if(check) {
      (void) pthread_mutex_lock(&epoll->lock);
      for(unsigned int i = 0; i < epoll->nets_used; ++i) {
        epoll->nets[i]->on_event(epoll->nets[i]);
      }
      if(epoll->nets != NULL) {
        free(epoll->nets);
        epoll->nets = NULL;
        epoll->nets_size = 0;
        epoll->nets_used = 0;
      }
      (void) pthread_mutex_unlock(&epoll->lock);
    }
    if(epoll->close) {
      (void) pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);
      net_epoll_stop(epoll);
      net_epoll_free(epoll);
      if(epoll->free) {
        free(epoll);
      }
      (void) pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
    }
  }
  return NULL;
}

#undef event_net

#define event_net ((struct net_socket*) epoll->events[i].data.ptr)

static void* net_epoll_thread_eventless(void* net_epoll_thread_data) {
  (void) pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL);
  while(1) {
    int count = epoll_wait(epoll->fd, epoll->events, epoll->events_size, -1);
    int check = 0;
    for(int i = 0; i < count; ++i) {
      if(event_net->wakeup) {
        uint64_t r;
        const int ret = eventfd_read(epoll->net.sfd, &r);
        (void) ret;
        check = 1;
        continue;
      }
      epoll->on_event(epoll, epoll->events[i].events, event_net);
    }
    if(check) {
      (void) pthread_mutex_lock(&epoll->lock);
      for(unsigned int i = 0; i < epoll->nets_used; ++i) {
        epoll->nets[i]->on_event(epoll->nets[i]);
      }
      if(epoll->nets != NULL) {
        free(epoll->nets);
        epoll->nets = NULL;
        epoll->nets_size = 0;
        epoll->nets_used = 0;
      }
      (void) pthread_mutex_unlock(&epoll->lock);
    }
    if(epoll->close) {
      (void) pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);
      net_epoll_stop(epoll);
      net_epoll_free(epoll);
      if(epoll->free) {
        free(epoll);
      }
      (void) pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
    }
  }
  return NULL;
}

#undef event_net
#undef epoll

int net_epoll(struct net_epoll* const epoll, const int support_wakeup_method) {
  int fd = epoll_create1(0);
  if(fd == -1) {
    return -1;
  }
  epoll->fd = fd;
  if(support_wakeup_method) {
    fd = pthread_mutex_init(&epoll->lock, NULL);
    if(fd != 0) {
      errno = fd;
      goto err_fd;
    }
    fd = eventfd(0, 0);
    if(fd == -1) {
      goto err_mutex;
    }
    epoll->net.sfd = fd;
    epoll->net.wakeup = 1;
    if(net_epoll_add(epoll, &epoll->net, EPOLLIN) != 0) {
      (void) close(epoll->net.sfd);
      goto err_mutex;
    }
  } else {
    epoll->net.sfd = -1;
  }
  return 0;
  
  err_mutex:
  pthread_mutex_destroy(&epoll->lock);
  err_fd:
  (void) close(epoll->fd);
  return -1;
}

int net_epoll_start(struct net_epoll* const epoll) {
  return thread_start(&epoll->thread, epoll->events == NULL ? net_epoll_thread : net_epoll_thread_eventless, epoll);
}

void net_epoll_stop(struct net_epoll* const epoll) {
  thread_stop(&epoll->thread);
}

void net_epoll_free(struct net_epoll* const epoll) {
  (void) close(epoll->fd);
  if(epoll->net.sfd != -1) {
    (void) close(epoll->net.sfd);
  }
  if(epoll->nets != NULL) {
    free(epoll->nets);
  }
}

static int net_epoll_modify(struct net_epoll* const epoll, void* const net, const int method, const int events) {
  return epoll_ctl(epoll->fd, method, ((struct net_socket*) net)->sfd, &((struct epoll_event) {
    .events = events,
    .data = (epoll_data_t) {
      .ptr = net
    }
  }));
}

int net_epoll_add(struct net_epoll* const epoll, void* const net, const int events) {
  return net_epoll_modify(epoll, net, EPOLL_CTL_ADD, events);
}

int net_epoll_mod(struct net_epoll* const epoll, void* const net, const int events) {
  return net_epoll_modify(epoll, net, EPOLL_CTL_MOD, events);
}

int net_epoll_remove(struct net_epoll* const epoll, void* const net) {
  return net_epoll_modify(epoll, net, EPOLL_CTL_DEL, 0);
}

static int net_epoll_resize_(struct net_epoll* const epoll, const uint32_t new_size) {
  struct net_server** const ptr = realloc(epoll->nets, sizeof(struct net_socket*) * new_size);
  if(ptr == NULL) {
    (void) pthread_mutex_unlock(&epoll->lock);
    return -1;
  }
  epoll->nets = ptr;
  epoll->nets_size = new_size;
  return 0;
}

int net_epoll_resize(struct net_epoll* const epoll, const uint32_t new_size) {
  (void) pthread_mutex_lock(&epoll->lock);
  const int ret = net_epoll_resize_(epoll, new_size);
  (void) pthread_mutex_unlock(&epoll->lock);
  return ret;
}

int net_epoll_create_event(struct net_epoll* const epoll, void* const net) {
  (void) pthread_mutex_lock(&epoll->lock);
  if(epoll->nets_used == epoll->nets_size && net_epoll_resize_(epoll, epoll->nets_used + 1) != 0) {
    return -1;
  }
  epoll->nets[epoll->nets_used++] = net;
  (void) pthread_mutex_unlock(&epoll->lock);
  const int ret = eventfd_write(epoll->net.sfd, 1);
  (void) ret;
  return 0;
}