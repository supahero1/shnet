#ifndef MHNJj_yfLA3WP__Eq_f4M__J_JwdkH_i
#define MHNJj_yfLA3WP__Eq_f4M__J_JwdkH_i 1

#include "threads.h"

#include <netdb.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <sys/eventfd.h>

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
  ipv6_strlen = INET6_ADDRSTRLEN,
  
  net_epoll_no_wakeup_method = 0,
  net_epoll_wakeup_method,
  
  net_unspecified = 0,
  net_wakeup_method,
  net_socket,
  net_server
};

extern struct addrinfo net_get_addr_struct(const int, const int, const int, const int);

extern struct addrinfo* net_get_address(const char* const, const char* const, const struct addrinfo* const);

struct net_async_address {
  char* hostname;
  char* service;
  struct addrinfo* hints;
  void (*callback)(struct net_async_address*, struct addrinfo*);
};

extern int net_get_address_async(struct net_async_address* const);

extern const char* net_strerror(const int);

extern void net_get_address_free(struct addrinfo* const);


extern int net_get_name(const void* const, const socklen_t, char* const, const socklen_t, const int);


extern int net_string_to_address(void* const, const char* const);

extern int net_address_to_string(void* const, char* const);

struct net_socket_base {
  int sfd;
  int events;
  struct sockaddr_in6 addr;
  int which;
  void (*onclose)(struct net_socket_base*);
};

extern void net_set_family(void* const, const int);

extern void net_sockbase_set_family(struct net_socket_base* const, const int);

extern void net_addrinfo_set_family(struct addrinfo* const, const int);


extern int net_get_family(const void* const);

extern int net_sockbase_get_family(const struct net_socket_base* const);

extern int net_addrinfo_get_family(const struct addrinfo* const);


extern void net_set_any_addr(void* const);

extern void net_sockbase_set_any_addr(struct net_socket_base* const);

extern void net_addrinfo_set_any_addr(struct addrinfo* const);


extern void net_set_loopback_addr(void* const);

extern void net_sockbase_set_loopback_addr(struct net_socket_base* const);

extern void net_addrinfo_set_loopback_addr(struct addrinfo* const);


extern void net_set_addr(void* const, const void* const);

extern void net_sockbase_set_addr(struct net_socket_base* const, const void* const);

extern void net_addrinfo_set_addr(struct addrinfo* const, const void* const);


extern void net_set_whole_addr(void* const, const void* const);

extern void net_sockbase_set_whole_addr(struct net_socket_base* const, const void* const);

extern void net_addrinfo_set_whole_addr(struct addrinfo* const, const void* const);


extern void* net_get_addr(const void* const);

extern void* net_sockbase_get_addr(const struct net_socket_base* const);

extern void* net_addrinfo_get_addr(const struct addrinfo* const);


extern void* net_sockbase_get_whole_addr(struct net_socket_base* const);

extern void* net_addrinfo_get_whole_addr(const struct addrinfo* const);


extern void net_set_port(void* const, const uint16_t);

extern void net_sockbase_set_port(struct net_socket_base* const, const uint16_t);

extern void net_addrinfo_set_port(struct addrinfo* const, const uint16_t);


extern uint16_t net_get_port(const void* const);

extern uint16_t net_sockbase_get_port(const struct net_socket_base* const);

extern uint16_t net_addrinfo_get_port(const struct addrinfo* const);


extern int net_get_ipv4_addrlen(void);

extern int net_get_ipv6_addrlen(void);

extern int net_get_addrlen(const void* const);

extern int net_addrinfo_get_addrlen(const struct addrinfo* const);


extern int net_get_socket(const struct addrinfo* const);

extern int net_bind_socket(const int, const void* const);

extern int net_connect_socket(const int sfd, const void* const);


extern int net_socket_setopt_true(const int, const int, const int);

extern int net_socket_setopt_false(const int, const int, const int);


extern int net_socket_reuse_addr(const int);

extern int net_socket_dont_reuse_addr(const int);

extern int net_socket_reuse_port(const int);

extern int net_socket_dont_reuse_port(const int);


extern int net_socket_get_family(const int, int* const);

extern int net_socket_get_socktype(const int, int* const);

extern int net_socket_get_protocol(const int, int* const);


extern int net_socket_dont_block(const int);

extern int net_socket_block(const int);

extern int net_socket_base_options(const int);


struct net_epoll {
  struct threads threads;
  struct net_socket_base base;
  void (*on_event)(struct net_epoll*, int, struct net_socket_base*);
  struct net_socket_base** bases_tbc;
  pthread_mutex_t lock;
  struct epoll_event* events;
  int events_size;
  int fd;
  uint32_t bases_tbc_used:31;
  uint32_t bases_tbc_allow_freeing:1;
  uint32_t bases_tbc_size;
};

extern int net_epoll(struct net_epoll* const, const int);

extern int net_epoll_start(struct net_epoll* const);

extern void net_epoll_stop(struct net_epoll* const);

extern void net_epoll_free(struct net_epoll* const);


extern int net_epoll_add(struct net_epoll* const, struct net_socket_base* const);

extern int net_epoll_mod(struct net_epoll* const, struct net_socket_base* const);

extern int net_epoll_remove(struct net_epoll* const, struct net_socket_base* const);


extern int net_epoll_tbc_resize(struct net_epoll* const, const uint32_t);

extern int net_epoll_safe_remove(struct net_epoll* const, struct net_socket_base* const);

#endif // MHNJj_yfLA3WP__Eq_f4M__J_JwdkH_i