#include "net.h"
#include "debug.h"
#include "error.h"

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
  int err;
  safe_execute(err = getaddrinfo(hostname, port, hints, &addr), err == EAI_MEMORY || err == EAI_SYSTEM, err == EAI_MEMORY ? ENOMEM : errno);
  if(err != 0) {
    if(err != EAI_SYSTEM) {
      errno = err;
    }
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
  int err;
  safe_execute(err = pthread_create(&id, NULL, net_get_address_thread, addr), err != 0, err);
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

int net_socket_get(const struct addrinfo* const info) {
  int err;
  safe_execute(err = socket(info->ai_family, info->ai_socktype, info->ai_protocol), err == -1, errno);
  return err;
}

int net_socket_bind(const struct net_socket* const socket, const struct addrinfo* const info) {
  int err;
  safe_execute(err = bind(socket->sfd, info->ai_addr, info->ai_family == ipv4 ? ipv4_size : ipv6_size), err == -1, errno);
  return err;
}

int net_socket_connect(const struct net_socket* const socket, const struct addrinfo* const info) {
  int err;
  safe_execute(err = connect(socket->sfd, info->ai_addr, info->ai_family == ipv4 ? ipv4_size : ipv6_size), err == -1, errno);
  return err;
}

int net_socket_setopt_true(const struct net_socket* const socket, const int level, const int option_name) {
  int err;
  safe_execute(err = setsockopt(socket->sfd, level, option_name, &(int){1}, sizeof(int)), err == -1, errno);
  return err;
}

int net_socket_setopt_false(const struct net_socket* const socket, const int level, const int option_name) {
  int err;
  safe_execute(err = setsockopt(socket->sfd, level, option_name, &(int){0}, sizeof(int)), err == -1, errno);
  return err;
}

/* The functions below don't take failure into account, because realistically
the only error is memory fault, which is the application's fault and not a real
error we should check for and prevent. It should never happen. */

void net_socket_reuse_addr(const struct net_socket* const socket) {
  (void) net_socket_setopt_true(socket, SOL_SOCKET, SO_REUSEADDR);
}

void net_socket_dont_reuse_addr(const struct net_socket* const socket) {
  (void) net_socket_setopt_false(socket, SOL_SOCKET, SO_REUSEADDR);
}

void net_socket_reuse_port(const struct net_socket* const socket) {
  (void) net_socket_setopt_true(socket, SOL_SOCKET, SO_REUSEPORT);
}

void net_socket_dont_reuse_port(const struct net_socket* const socket) {
  (void) net_socket_setopt_false(socket, SOL_SOCKET, SO_REUSEPORT);
}

void net_socket_get_family(const struct net_socket* const socket, int* const family) {
  (void) getsockopt(socket->sfd, SOL_SOCKET, SO_DOMAIN, family, &(socklen_t){sizeof(int)});
}

void net_socket_get_socktype(const struct net_socket* const socket, int* const socktype) {
  (void) getsockopt(socket->sfd, SOL_SOCKET, SO_TYPE, socktype, &(socklen_t){sizeof(int)});
}

void net_socket_get_protocol(const struct net_socket* const socket, int* const protocol) {
  (void) getsockopt(socket->sfd, SOL_SOCKET, SO_PROTOCOL, protocol, &(socklen_t){sizeof(int)});
}

void net_socket_get_address(const struct net_socket* const socket, void* const address) {
  (void) getpeername(socket->sfd, address, &(socklen_t){sizeof(struct sockaddr_in6)});
}

void net_socket_dont_block(const struct net_socket* const socket) {
  (void) fcntl(socket->sfd, F_SETFL, fcntl(socket->sfd, F_GETFL, 0) | O_NONBLOCK);
}

void net_socket_block(const struct net_socket* const socket) {
  (void) fcntl(socket->sfd, F_SETFL, fcntl(socket->sfd, F_GETFL, 0) & ~O_NONBLOCK);
}

void net_socket_default_options(const struct net_socket* const socket) {
  net_socket_reuse_addr(socket);
  net_socket_reuse_port(socket);
  net_socket_dont_block(socket);
}



#define epoll ((struct net_epoll*) net_epoll_thread_data)
#define event_net ((struct net_socket*) events[i].data.ptr)

void* net_epoll_thread(void* net_epoll_thread_data) {
  (void) pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL);
  struct epoll_event events[NET_EPOLL_DEFAULT_MAX_EVENTS];
  while(1) {
    const int count = epoll_wait(epoll->fd, events, NET_EPOLL_DEFAULT_MAX_EVENTS, -1);
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
      if(epoll->nets != NULL) {
        for(uint32_t i = 0; i < epoll->nets_len; ++i) {
          epoll->on_event(epoll, -1, epoll->nets[i]);
        }
        free(epoll->nets);
        epoll->nets = NULL;
        epoll->nets_len = 0;
      }
      (void) pthread_mutex_unlock(&epoll->lock);
    }
  }
  return NULL;
}

#undef event_net

#define event_net ((struct net_socket*) epoll->events[i].data.ptr)

void* net_epoll_thread_eventless(void* net_epoll_thread_data) {
  (void) pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL);
  while(1) {
    const int count = epoll_wait(epoll->fd, epoll->events, epoll->events_size, -1);
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
      if(epoll->nets != NULL) {
        for(uint32_t i = 0; i < epoll->nets_len; ++i) {
          epoll->on_event(epoll, -1, epoll->nets[i]);
        }
        free(epoll->nets);
        epoll->nets = NULL;
        epoll->nets_len = 0;
      }
      (void) pthread_mutex_unlock(&epoll->lock);
    }
  }
  return NULL;
}

#undef event_net
#undef epoll

static int net_epoll(struct net_epoll* const epoll, const int support_wakeup_method) {
  /* Looks like a net to me */
  {
    int fd;
    safe_execute(fd = epoll_create1(0), fd == -1, errno);
    if(fd == -1) {
      return -1;
    }
    epoll->fd = fd;
  }
  if(support_wakeup_method) {
    {
      int err;
      safe_execute(err = pthread_mutex_init(&epoll->lock, NULL), err != 0, err);
      if(err != 0) {
        errno = err;
        goto err_fd;
      }
    }
    {
      int fd;
      safe_execute(fd = eventfd(0, EFD_SEMAPHORE), fd == -1, errno);
      if(fd == -1) {
        goto err_mutex;
      }
      epoll->net.sfd = fd;
    }
    epoll->net.wakeup = 1;
    if(net_epoll_add(epoll, &epoll->net, EPOLLIN) != 0) {
      goto err_efd;
    }
  } else {
    epoll->net.sfd = -1;
  }
  return 0;
  
  err_efd:
  (void) close(epoll->net.sfd);
  err_mutex:
  (void) pthread_mutex_destroy(&epoll->lock);
  err_fd:
  (void) close(epoll->fd);
  return -1;
}

int net_socket_epoll(struct net_epoll* const epoll) {
  return net_epoll(epoll, 0);
}

int net_server_epoll(struct net_epoll* const epoll) {
  return net_epoll(epoll, 1);
}

int net_epoll_start(struct net_epoll* const epoll) {
  return thread_start(&epoll->thread, epoll->events == NULL ? net_epoll_thread : net_epoll_thread_eventless, epoll);
}

void net_epoll_stop(struct net_epoll* const epoll) {
  thread_stop(&epoll->thread);
}

void net_epoll_stop_async(struct net_epoll* const epoll) {
  thread_stop_async(&epoll->thread);
}

void net_epoll_free(struct net_epoll* const epoll) {
  (void) close(epoll->fd);
  (void) close(epoll->net.sfd);
  if(epoll->net.wakeup) {
    (void) pthread_mutex_destroy(&epoll->lock);
  }
  if(epoll->nets != NULL) {
    free(epoll->nets);
    epoll->nets = 0;
    epoll->nets_len = 0;
  }
}

void net_epoll_shutdown(struct net_epoll* const epoll, const int _free) {
  (void) pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);
  net_epoll_stop(epoll);
  net_epoll_free(epoll);
  if(_free) {
    free(epoll);
  }
  (void) pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
}

static int net_epoll_modify(struct net_epoll* const epoll, void* const net, const int method, const uint32_t events) {
  int err;
  safe_execute(err = epoll_ctl(epoll->fd, method, ((struct net_socket*) net)->sfd, &((struct epoll_event) {
    .events = events,
    .data = (epoll_data_t) {
      .ptr = net
    }
  })), err == -1, errno);
  return err;
}

int net_epoll_add(struct net_epoll* const epoll, void* const net, const uint32_t events) {
  return net_epoll_modify(epoll, net, EPOLL_CTL_ADD, events);
}

int net_epoll_mod(struct net_epoll* const epoll, void* const net, const uint32_t events) {
  return net_epoll_modify(epoll, net, EPOLL_CTL_MOD, events);
}

int net_epoll_remove(struct net_epoll* const epoll, void* const net) {
  return net_epoll_modify(epoll, net, EPOLL_CTL_DEL, 0);
}

int net_epoll_create_event(struct net_epoll* const epoll, void* const net) {
  (void) pthread_mutex_lock(&epoll->lock);
  {
    struct net_socket** ptr;
    safe_execute(ptr = realloc(epoll->nets, sizeof(struct net_socket*) * (epoll->nets_len + 1)), ptr == NULL, ENOMEM);
    if(ptr == NULL) {
      (void) pthread_mutex_unlock(&epoll->lock);
      return -1;
    }
    epoll->nets = ptr;
  }
  epoll->nets[epoll->nets_len++] = net;
  (void) pthread_mutex_unlock(&epoll->lock);
  const int ret = eventfd_write(epoll->net.sfd, 1);
  (void) ret;
  return 0;
}