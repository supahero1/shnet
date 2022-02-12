#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <endian.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/ip.h>

#include <shnet/net.h>
#include <shnet/error.h>

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
  safe_execute(err = getaddrinfo(hostname, port, hints, &addr), err != 0, err == EAI_SYSTEM ? errno : err);
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



void net_address_to_string(const void* const addr, char* const buffer) {
  if(net_address_to_family(addr) == net_family_ipv4) {
    (void) inet_ntop(net_family_ipv4, &((struct sockaddr_in*) addr)->sin_addr.s_addr, buffer, net_const_ipv4_size);
  } else {
    (void) inet_ntop(net_family_ipv6, ((struct sockaddr_in6*) addr)->sin6_addr.s6_addr, buffer, net_const_ipv6_size);
  }
}

sa_family_t net_address_to_family(const void* const addr) {
  return ((struct sockaddr*) addr)->sa_family;
}

uint16_t net_address_to_port(const void* const addr) {
  if(net_address_to_family(addr) == net_family_ipv4) {
    return ntohs(((struct sockaddr_in*) addr)->sin_port);
  } else {
    return ntohs(((struct sockaddr_in6*) addr)->sin6_port);
  }
}

void* net_address_to_ip(const void* const addr) {
  if(net_address_to_family(addr) == net_family_ipv4) {
    return &((struct sockaddr_in*) addr)->sin_addr.s_addr;
  } else {
    return ((struct sockaddr_in6*) addr)->sin6_addr.s6_addr;
  }
}



int net_socket_get(const struct addrinfo* const info) {
  int err;
  safe_execute(err = socket(info->ai_family, info->ai_socktype, info->ai_protocol), err == -1, errno);
  return err;
}

int net_socket_bind(const int sfd, const struct addrinfo* const info) {
  int err;
  safe_execute(err = bind(sfd, info->ai_addr, info->ai_family == net_family_ipv4 ? net_const_ipv4_size : net_const_ipv6_size), err == -1 && (errno == ENOMEM || errno == ENOBUFS), errno);
  return err;
}

int net_socket_connect(const int sfd, const struct addrinfo* const info) {
  int err;
  safe_execute(err = connect(sfd, info->ai_addr, info->ai_family == net_family_ipv4 ? net_const_ipv4_size : net_const_ipv6_size), err == -1 && (errno == ENOMEM || errno == ENOBUFS), errno);
  return err;
}

int net_socket_setopt_true(const int sfd, const int level, const int option_name) {
  int err;
  safe_execute(err = setsockopt(sfd, level, option_name, &(int){1}, sizeof(int)), err == -1, errno);
  return err;
}

int net_socket_setopt_false(const int sfd, const int level, const int option_name) {
  int err;
  safe_execute(err = setsockopt(sfd, level, option_name, &(int){0}, sizeof(int)), err == -1, errno);
  return err;
}

void net_socket_reuse_addr(const int sfd) {
  (void) net_socket_setopt_true(sfd, SOL_SOCKET, SO_REUSEADDR);
}

void net_socket_dont_reuse_addr(const int sfd) {
  (void) net_socket_setopt_false(sfd, SOL_SOCKET, SO_REUSEADDR);
}

void net_socket_reuse_port(const int sfd) {
  (void) net_socket_setopt_true(sfd, SOL_SOCKET, SO_REUSEPORT);
}

void net_socket_dont_reuse_port(const int sfd) {
  (void) net_socket_setopt_false(sfd, SOL_SOCKET, SO_REUSEPORT);
}

int net_socket_get_family(const int sfd) {
  int ret;
  (void) getsockopt(sfd, SOL_SOCKET, SO_DOMAIN, &ret, &(socklen_t){sizeof(int)});
  return ret;
}

int net_socket_get_socktype(const int sfd) {
  int ret;
  (void) getsockopt(sfd, SOL_SOCKET, SO_TYPE, &ret, &(socklen_t){sizeof(int)});
  return ret;
}

int net_socket_get_protocol(const int sfd) {
  int ret;
  (void) getsockopt(sfd, SOL_SOCKET, SO_PROTOCOL, &ret, &(socklen_t){sizeof(int)});
  return ret;
}

void net_socket_get_peer_address(const int sfd, void* const address) {
  (void) getpeername(sfd, address, &(socklen_t){sizeof(struct sockaddr_in6)});
}

void net_socket_get_local_address(const int sfd, void* const address) {
  (void) getsockname(sfd, address, &(socklen_t){sizeof(struct sockaddr_in6)});
}

void net_socket_dont_block(const int sfd) {
  (void) fcntl(sfd, F_SETFL, fcntl(sfd, F_GETFL, 0) | O_NONBLOCK);
}

void net_socket_default_options(const int sfd) {
  net_socket_reuse_addr(sfd);
  net_socket_reuse_port(sfd);
  net_socket_dont_block(sfd);
}