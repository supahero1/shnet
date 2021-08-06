#ifndef MHNJj_yfLA3WP__Eq_f4M__J_JwdkH_i
#define MHNJj_yfLA3WP__Eq_f4M__J_JwdkH_i 1

#include "threads.h"

#include <netdb.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/epoll.h>
#include <sys/socket.h>

/* The rules of shnet sockets:
1. A socket or a server can ONLY be in 1 epoll at a time
2. All server sockets must be in the same epoll as their server
3. A socket or a server may only be used from 1 thread at a time (excluding epoll)
*/

#define NET_EPOLL_DEFAULT_MAX_EVENTS 100

enum net_consts {
  /* FAMILY */
  any_family = AF_UNSPEC,
  ipv4 = AF_INET,
  ipv6 = AF_INET6,
  
  /* SOCKTYPE */
  any_socktype = 0,
  stream_socktype = SOCK_STREAM,
  datagram_socktype = SOCK_DGRAM,
  raw_socktype = SOCK_RAW,
  seqpacket_socktype = SOCK_SEQPACKET,
  
  /* PROTOCOL */
  any_protocol = 0,
  tcp_protocol = IPPROTO_TCP,
  udp_protocol = IPPROTO_UDP,
  udp_lite_protocol = IPPROTO_UDPLITE,
  
  /* FLAGS */
  wants_a_server = AI_PASSIVE,
  wants_canonical_name = AI_CANONNAME,
  numeric_hostname = AI_NUMERICHOST,
  numeric_service = AI_NUMERICSERV,
  wants_own_ip_version = AI_ADDRCONFIG,
  wants_all_addresses = AI_ALL,
  wants_mapped_ipv4 = AI_V4MAPPED,
  
  ip_max_strlen = INET6_ADDRSTRLEN,
  ipv4_strlen = INET_ADDRSTRLEN,
  ipv6_strlen = INET6_ADDRSTRLEN
};

extern struct addrinfo  net_get_addr_struct(const int, const int, const int, const int);

extern struct addrinfo* net_get_address(const char* const, const char* const, const struct addrinfo* const);

struct net_async_address {
  const char* hostname;
  const char* port;
  void* data;
  struct addrinfo* hints;
  void (*callback)(struct net_async_address*, struct addrinfo*);
};

extern int  net_get_address_async(struct net_async_address* const);

extern void net_free_address(struct addrinfo* const);

extern int  net_address_to_string(void* const, char* const);

struct net_socket {
  unsigned int secure:1;
  unsigned int socket:1;
  unsigned int wakeup:1;
  int sfd;
};

struct net_server {
  struct net_socket net;
  void (*on_event)(void*);
};

extern int net_get_ipv4_addrlen(void);

extern int net_get_ipv6_addrlen(void);

extern int net_socket_get(const struct addrinfo* const);

extern int net_socket_bind(const void* const, const struct addrinfo* const);

extern int net_socket_connect(const void* const, const struct addrinfo* const);

extern int net_socket_setopt_true(const void* const, const int, const int);

extern int net_socket_setopt_false(const void* const, const int, const int);

extern int net_socket_reuse_addr(const void* const);

extern int net_socket_dont_reuse_addr(const void* const);

extern int net_socket_reuse_port(const void* const);

extern int net_socket_dont_reuse_port(const void* const);

extern int net_socket_get_family(const void* const, int* const);

extern int net_socket_get_socktype(const void* const, int* const);

extern int net_socket_get_protocol(const void* const, int* const);

extern int net_socket_get_address(const void* const, void* const);

extern int net_socket_dont_block(const void* const);

extern int net_socket_block(const void* const);

extern int net_socket_default_options(const void* const);

struct net_epoll {
  struct net_server** nets;
  struct epoll_event* events;
  void (*on_event)(struct net_epoll*, int, struct net_socket*);
  
  struct thread thread;
  struct net_socket net;
  pthread_mutex_t lock;
  
  uint32_t nets_used:31;
  uint32_t close:1;
  uint32_t nets_size:31;
  uint32_t free:1;
  int events_size;
  int fd;
};

extern int  net_epoll(struct net_epoll* const, const int);

extern int  net_epoll_start(struct net_epoll* const);

extern void net_epoll_stop(struct net_epoll* const);

extern void net_epoll_free(struct net_epoll* const);

extern int  net_epoll_add(struct net_epoll* const, void* const, const int);

extern int  net_epoll_mod(struct net_epoll* const, void* const, const int);

extern int  net_epoll_remove(struct net_epoll* const, void* const);

extern int  net_epoll_resize(struct net_epoll* const, const uint32_t);

extern int  net_epoll_create_event(struct net_epoll* const, void* const);

#endif // MHNJj_yfLA3WP__Eq_f4M__J_JwdkH_i