#ifndef MHNJj_yfLA3WP__Eq_f4M__J_JwdkH_i
#define MHNJj_yfLA3WP__Eq_f4M__J_JwdkH_i 1

#include "misc.h"
#include "threads.h"

#include <netdb.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/epoll.h>
#include <sys/socket.h>

enum net_consts {
  net_success,
  net_out_of_memory,
  net_failure,
  
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
  wants_mapped_ipv4 = AI_V4MAPPED
};

extern struct addrinfo net_get_addr_struct(const int, const int, const int, const int);

extern struct addrinfo* net_get_address(const char* const, const char* const, const struct addrinfo* const);

extern const char* net_get_address_strerror(const int);

extern void net_get_address_free(struct addrinfo* const);

extern int net_get_name(const void* const, const socklen_t, char* const, const socklen_t, const int);

extern int net_string_to_address(void* const, const char* const);

extern int net_address_to_string(void* const, char* const);

extern void net_set_family(void* const, const int);

extern void net_any_address(void* const);

extern void net_set_address(void* const, const void* const);

extern void net_set_port(void* const, const in_port_t);

extern in_port_t net_get_port(void* const);

extern int net_get_ipv4_addrlen(void);

extern int net_get_ipv6_addrlen(void);

extern int net_get_addrlen(const void* const);

extern int net_get_socket(const struct addrinfo* const);

extern int net_bind_socket(const int, const void* const);

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

struct net_epoll {
  struct threads thread;
  int sfd;
};

extern int net_epoll(struct net_epoll* const);

extern int net_epoll_start(struct net_epoll* const);

extern void net_epoll_stop(struct net_epoll* const);

extern void net_epoll_free(struct net_epoll* const);

struct net_server_base {
  int sfd;
  int events;
  struct contmem sockets;
  struct mufex mutex;
  void (*on_event)(struct net_epoll*, struct epoll_event);
};

extern int net_server_base(struct net_server_base* const, const unsigned long, const unsigned long, const unsigned long, void (*)(struct net_epoll*, struct epoll_event));

extern void net_server_base_free(struct net_server_base* const);

struct net_socket_base {
  int sfd;
  int events;
  struct net_server_base* server;
};

extern int net_epoll_add_socket(struct net_epoll* const, struct net_socket_base* const);

extern int net_epoll_mod_socket(struct net_epoll* const, struct net_socket_base* const);

extern int net_epoll_remove_socket(struct net_epoll* const, struct net_socket_base* const);

extern int net_epoll_add_server(struct net_epoll* const, struct net_server_base* const);

extern int net_epoll_mod_server(struct net_epoll* const, struct net_server_base* const);

extern int net_epoll_remove_server(struct net_epoll* const, struct net_server_base* const);

#endif // MHNJj_yfLA3WP__Eq_f4M__J_JwdkH_i