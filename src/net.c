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

struct addrinfo* net_get_address(const char* const hostname, const char* const service, const struct addrinfo* const hints) {
  struct addrinfo* addr;
  int err = getaddrinfo(hostname, service, hints, &addr);
  if(err != 0) {
    errno = err;
    return NULL;
  }
  return addr;
}

#define addr ((struct net_async_address*) net_get_address_thread_data)

static void* net_get_address_thread(void* net_get_address_thread_data) {
  addr->callback(addr, net_get_address(addr->hostname, addr->service, addr->hints));
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

const char* net_strerror(const int code) {
  return gai_strerror(code);
}

void net_get_address_free(struct addrinfo* const info) {
  freeaddrinfo(info);
}

int net_foreach_addrinfo(struct addrinfo* info, int (*callback)(struct addrinfo*, void*), void* data) {
  do {
    if(callback(info, data) == 0) {
      return 0;
    }
    info = info->ai_next;
  } while(info != NULL);
  if(info == NULL) {
    return -1;
  }
  return 0;
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
  if(err != 1) {
    return -1;
  }
  return 0;
}

int net_address_to_string(void* const whole_addr, char* const buffer) {
  const void* err;
  if(((struct sockaddr*) whole_addr)->sa_family == ipv4) {
    err = inet_ntop(ipv4, &((struct sockaddr_in*) whole_addr)->sin_addr.s_addr, buffer, ipv4_strlen);
  } else {
    err = inet_ntop(ipv6, ((struct sockaddr_in6*) whole_addr)->sin6_addr.s6_addr, buffer, ipv6_strlen);
  }
  if(err == NULL) {
    return -1;
  }
  return 0;
}



void net_set_family(void* const whole_addr, const int family) {
  ((struct sockaddr*) whole_addr)->sa_family = family;
}

void net_sockbase_set_family(struct net_socket_base* const base, const int family) {
  net_set_family(&base->addr, family);
}

void net_addrinfo_set_family(struct addrinfo* const info, const int family) {
  net_set_family(info->ai_addr, family);
  info->ai_family = family;
}



int net_get_family(const void* const whole_addr) {
  return ((struct sockaddr*) whole_addr)->sa_family;
}

int net_sockbase_get_family(const struct net_socket_base* const base) {
  return net_get_family(&base->addr);
}

int net_addrinfo_get_family(const struct addrinfo* const info) {
  return net_get_family(info->ai_addr);
}



void net_set_any_addr(void* const whole_addr) {
  if(((struct sockaddr*) whole_addr)->sa_family == ipv4) {
    ((struct sockaddr_in*) whole_addr)->sin_addr.s_addr = INADDR_ANY;
  } else {
    ((struct sockaddr_in6*) whole_addr)->sin6_addr = in6addr_any;
  }
}

void net_sockbase_set_any_addr(struct net_socket_base* const base) {
  net_set_any_addr(&base->addr);
}

void net_addrinfo_set_any_addr(struct addrinfo* const info) {
  net_set_any_addr(info->ai_addr);
}



void net_set_loopback_addr(void* const whole_addr) {
  if(((struct sockaddr*) whole_addr)->sa_family == ipv4) {
    ((struct sockaddr_in*) whole_addr)->sin_addr.s_addr = INADDR_LOOPBACK;
  } else {
    ((struct sockaddr_in6*) whole_addr)->sin6_addr = in6addr_loopback;
  }
}

void net_sockbase_set_loopback_addr(struct net_socket_base* const base) {
  net_set_loopback_addr(&base->addr);
}

void net_addrinfo_set_loopback_addr(struct addrinfo* const info) {
  net_set_loopback_addr(info->ai_addr);
}



void net_set_addr(void* const whole_addr, const void* const addr) {
  if(((struct sockaddr*) whole_addr)->sa_family == ipv4) {
    (void) memcpy(&((struct sockaddr_in*) whole_addr)->sin_addr.s_addr, addr, 4);
  } else {
    (void) memcpy(((struct sockaddr_in6*) whole_addr)->sin6_addr.s6_addr, addr, 16);
  }
}

void net_sockbase_set_addr(struct net_socket_base* const base, const void* const addr) {
  net_set_addr(&base->addr, addr);
}

void net_addrinfo_set_addr(struct addrinfo* const info, const void* const addr) {
  net_set_addr(info->ai_addr, addr);
}



void net_set_whole_addr(void* const whole_addr, const void* const addr) {
  if(((struct sockaddr*) addr)->sa_family == ipv4) {
    (void) memcpy(((struct sockaddr_in*) whole_addr), addr, sizeof(struct sockaddr_in));
  } else {
    (void) memcpy(((struct sockaddr_in6*) whole_addr), addr, sizeof(struct sockaddr_in6));
  }
}

void net_sockbase_set_whole_addr(struct net_socket_base* const base, const void* const addr) {
  net_set_whole_addr(&base->addr, addr);
}

void net_addrinfo_set_whole_addr(struct addrinfo* const info, const void* const addr) {
  net_set_whole_addr(info->ai_addr, addr);
  info->ai_family = net_get_family(addr);
}



void* net_get_addr(const void* const whole_addr) {
  if(((struct sockaddr*) whole_addr)->sa_family == ipv4) {
    return &((struct sockaddr_in*) whole_addr)->sin_addr.s_addr;
  } else {
    return ((struct sockaddr_in6*) whole_addr)->sin6_addr.s6_addr;
  }
}

void* net_sockbase_get_addr(const struct net_socket_base* const base) {
  return net_get_addr(&base->addr);
}

void* net_addrinfo_get_addr(const struct addrinfo* const info) {
  return net_get_addr(info->ai_addr);
}



void* net_sockbase_get_whole_addr(struct net_socket_base* const base) {
  return &base->addr;
}

void* net_addrinfo_get_whole_addr(const struct addrinfo* const info) {
  return info->ai_addr;
}



void net_set_port(void* const whole_addr, const uint16_t port) {
  if(((struct sockaddr*) whole_addr)->sa_family == ipv4) {
    ((struct sockaddr_in*) whole_addr)->sin_port = htons(port);
  } else {
    ((struct sockaddr_in6*) whole_addr)->sin6_port = htons(port);
  }
}

void net_sockbase_set_port(struct net_socket_base* const base, const uint16_t port) {
  net_set_port(&base->addr, port);
}

void net_addrinfo_set_port(struct addrinfo* const info, const uint16_t port) {
  net_set_port(info->ai_addr, port);
}



uint16_t net_get_port(const void* const whole_addr) {
  if(((struct sockaddr*) whole_addr)->sa_family == ipv4) {
    return ntohs(((struct sockaddr_in*) whole_addr)->sin_port);
  } else {
    return ntohs(((struct sockaddr_in6*) whole_addr)->sin6_port);
  }
}

uint16_t net_sockbase_get_port(const struct net_socket_base* const base) {
  return net_get_port(&base->addr);
}

uint16_t net_addrinfo_get_port(const struct addrinfo* const info) {
  return net_get_port(info->ai_addr);
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

int net_addrinfo_get_addrlen(const struct addrinfo* const info) {
  return info->ai_addrlen;
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
    return 0;
  }
  return -1;
}

int net_socket_setopt_false(const int sfd, const int level, const int option_name) {
  if(setsockopt(sfd, level, option_name, &(int){0}, sizeof(int)) == 0) {
    return 0;
  }
  return -1;
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
    return -1;
  }
  const int res = fcntl(sfd, F_SETFL, flags | O_NONBLOCK);
  if(res == -1) {
    return -1;
  }
  return 0;
}

int net_socket_block(const int sfd) {
  int flags = fcntl(sfd, F_GETFL, 0);
  if(flags == -1) {
    return -1;
  }
  if((flags ^ O_NONBLOCK) != 0) {
    flags ^= O_NONBLOCK;
  }
  const int res = fcntl(sfd, F_SETFL, flags);
  if(res == -1) {
    return -1;
  }
  return 0;
}

int net_socket_base_options(const int sfd) {
  if(net_socket_dont_block(sfd) != 0 ||
     net_socket_reuse_addr(sfd) != 0 ||
     net_socket_reuse_port(sfd) != 0) {
    return -1;
  }
  return 0;
}



#define epoll ((struct net_epoll*) net_epoll_thread_data)

static void net_epoll_thread(void* net_epoll_thread_data) {
  (void) pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL);
  pthread_testcancel();
  {
    sigset_t mask;
    (void) sigfillset(&mask);
    (void) pthread_sigmask(SIG_BLOCK, &mask, NULL);
  }
  struct epoll_event events[NET_EPOLL_DEFAULT_MAX_EVENTS];
  while(1) {
    int count = epoll_wait(epoll->fd, events, NET_EPOLL_DEFAULT_MAX_EVENTS, -1);
    for(int i = 0; i < count; ++i) {
      if(((struct net_socket_base*) events[i].data.ptr)->which == net_wakeup_method) {
        uint64_t r;
        const int ret = eventfd_read(epoll->base.sfd, &r);
        (void) ret;
        continue;
      }
      epoll->on_event(epoll, events[i].events, (struct net_socket_base*) events[i].data.ptr);
      (void) pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
    }
    (void) pthread_mutex_lock(&epoll->lock);
    for(uint32_t i = 0; i < epoll->bases_tbc_used; ++i) {
      epoll->bases_tbc[i]->onclose(epoll->bases_tbc[i]);
    }
    if(epoll->bases_tbc_allow_freeing == 1) {
      free(epoll->bases_tbc);
      epoll->bases_tbc = NULL;
      epoll->bases_tbc_size = 0;
    }
    epoll->bases_tbc_used = 0;
    (void) pthread_mutex_unlock(&epoll->lock);
  }
}

static void net_epoll_thread_eventless(void* net_epoll_thread_data) {
  (void) pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL);
  pthread_testcancel();
  {
    sigset_t mask;
    (void) sigfillset(&mask);
    (void) pthread_sigmask(SIG_BLOCK, &mask, NULL);
  }
  while(1) {
    int count = epoll_wait(epoll->fd, epoll->events, epoll->events_size, -1);
    for(int i = 0; i < count; ++i) {
      if(((struct net_socket_base*) epoll->events[i].data.ptr)->which == net_wakeup_method) {
        uint64_t r;
        const int ret = eventfd_read(epoll->base.sfd, &r);
        (void) ret;
        continue;
      }
      epoll->on_event(epoll, epoll->events[i].events, (struct net_socket_base*) epoll->events[i].data.ptr);
      (void) pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
    }
    (void) pthread_mutex_lock(&epoll->lock);
    for(uint32_t i = 0; i < epoll->bases_tbc_used; ++i) {
      epoll->bases_tbc[i]->onclose(epoll->bases_tbc[i]);
    }
    if(epoll->bases_tbc_allow_freeing == 1) {
      free(epoll->bases_tbc);
      epoll->bases_tbc = NULL;
      epoll->bases_tbc_size = 0;
    }
    epoll->bases_tbc_used = 0;
    (void) pthread_mutex_unlock(&epoll->lock);
  }
}

#undef epoll

int net_epoll(struct net_epoll* const epoll, const int wakeup_method) {
  if(threads(&epoll->threads) != 0) {
    return -1;
  }
  int fd = epoll_create1(0);
  if(fd == -1) {
    goto err_threads;
  }
  epoll->fd = fd;
  if(wakeup_method == net_epoll_wakeup_method) {
    fd = pthread_mutex_init(&epoll->lock, NULL);
    if(fd != 0) {
      errno = fd;
      goto err_fd;
    }
    fd = eventfd(0, 0);
    if(fd == -1) {
      goto err_mutex;
    }
    epoll->base.events = EPOLLIN;
    epoll->base.which = net_wakeup_method;
    epoll->base.sfd = fd;
    if(net_epoll_add(epoll, &epoll->base) != 0) {
      (void) close(epoll->base.sfd);
      goto err_mutex;
    }
  }
  if(epoll->events == NULL) {
    epoll->threads.func = net_epoll_thread;
  } else {
    epoll->threads.func = net_epoll_thread_eventless;
  }
  epoll->threads.data = epoll;
  return 0;
  
  err_mutex:
  pthread_mutex_destroy(&epoll->lock);
  err_fd:
  (void) close(epoll->fd);
  err_threads:
  threads_free(&epoll->threads);
  return -1;
}

int net_epoll_start(struct net_epoll* const epoll) {
  return threads_add(&epoll->threads, 1);
}

void net_epoll_stop(struct net_epoll* const epoll) {
  threads_shutdown(&epoll->threads);
}

void net_epoll_free(struct net_epoll* const epoll) {
  threads_free(&epoll->threads);
  (void) close(epoll->fd);
  (void) close(epoll->base.sfd);
  free(epoll->bases_tbc);
}

static int net_epoll_modify(struct net_epoll* const epoll, struct net_socket_base* const base, const int method) {
  return epoll_ctl(epoll->fd, method, base->sfd, &((struct epoll_event) {
    .events = base->events,
    .data = (epoll_data_t) {
      .ptr = base
    }
  }));
}

int net_epoll_add(struct net_epoll* const epoll, struct net_socket_base* const base) {
  return net_epoll_modify(epoll, base, EPOLL_CTL_ADD);
}

int net_epoll_mod(struct net_epoll* const epoll, struct net_socket_base* const base) {
  return net_epoll_modify(epoll, base, EPOLL_CTL_MOD);
}

int net_epoll_remove(struct net_epoll* const epoll, struct net_socket_base* const base) {
  return net_epoll_modify(epoll, base, EPOLL_CTL_DEL);
}



int net_epoll_tbc_resize(struct net_epoll* const epoll, const uint32_t new_size) {
  (void) pthread_mutex_lock(&epoll->lock);
  struct net_socket_base** const ptr = realloc(epoll->bases_tbc, sizeof(struct net_socket_base*) * new_size);
  if(ptr == NULL) {
    (void) pthread_mutex_unlock(&epoll->lock);
    return -1;
  }
  epoll->bases_tbc = ptr;
  ++epoll->bases_tbc_size;
  (void) pthread_mutex_unlock(&epoll->lock);
  return 0;
}

/* This will ONLY work when the base is in 1 epoll */

int net_epoll_safe_remove(struct net_epoll* const epoll, struct net_socket_base* const base) {
  (void) pthread_mutex_lock(&epoll->lock);
  if(epoll->bases_tbc_used == epoll->bases_tbc_size) {
    struct net_socket_base** const ptr = realloc(epoll->bases_tbc, sizeof(struct net_socket_base*) * (epoll->bases_tbc_size + 1));
    if(ptr == NULL) {
      (void) pthread_mutex_unlock(&epoll->lock);
      return -1;
    }
    epoll->bases_tbc = ptr;
    ++epoll->bases_tbc_size;
  }
  epoll->bases_tbc[epoll->bases_tbc_used++] = base;
  (void) pthread_mutex_unlock(&epoll->lock);
  const int ret = eventfd_write(epoll->base.sfd, 1);
  (void) ret;
  return 0;
}