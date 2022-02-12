#ifndef MHNJj_yfLA3WP__Eq_f4M__J_JwdkH_i
#define MHNJj_yfLA3WP__Eq_f4M__J_JwdkH_i 1

#include <netdb.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/socket.h>

enum net_consts {
  /* FAMILIES */
  net_family_any = AF_UNSPEC,
  net_family_ipv4 = AF_INET,
  net_family_ipv6 = AF_INET6,
  
  /* SOCKTYPES */
  net_sock_any = 0,
  net_sock_stream = SOCK_STREAM,
  net_sock_datagram = SOCK_DGRAM,
  net_sock_raw = SOCK_RAW,
  net_sock_seqpacket = SOCK_SEQPACKET,
  
  /* PROTOCOLS */
  net_proto_any = 0,
  net_proto_tcp = IPPROTO_TCP,
  net_proto_udp = IPPROTO_UDP,
  net_proto_udp_lite = IPPROTO_UDPLITE,
  
  /* FLAGS */
  net_flag_wants_server = AI_PASSIVE,
  net_flag_wants_canonical_name = AI_CANONNAME,
  net_flag_numeric_hostname = AI_NUMERICHOST,
  net_flag_numeric_service = AI_NUMERICSERV,
  net_flag_wants_own_ip_version = AI_ADDRCONFIG,
  net_flag_wants_all_addresses = AI_ALL,
  net_flag_wants_mapped_ipv4 = AI_V4MAPPED,
  
  /* CONSTANTS */
  net_const_ipv4_strlen = INET_ADDRSTRLEN,
  net_const_ipv6_strlen = INET6_ADDRSTRLEN,
  net_const_ip_max_strlen = net_const_ipv6_strlen,
  net_const_ipv4_size = sizeof(struct sockaddr_in),
  net_const_ipv6_size = sizeof(struct sockaddr_in6),
  net_const_ip_max_size = net_const_ipv6_size
};

extern struct addrinfo  net_get_addr_struct(const int, const int, const int, const int);

extern struct addrinfo* net_get_address(const char* const, const char* const, const struct addrinfo* const);

struct net_async_address {
  char* hostname;
  char* port;
  struct addrinfo* hints;
  void* data;
  void (*callback)(struct net_async_address*, struct addrinfo*);
};

extern int   net_get_address_async(struct net_async_address* const);

extern void  net_free_address(struct addrinfo* const);


extern void  net_address_to_string(const void* const, char* const);

extern sa_family_t net_address_to_family(const void* const);

extern uint16_t net_address_to_port(const void* const);

extern void* net_address_to_ip(const void* const);


extern int  net_socket_get(const struct addrinfo* const);

extern int  net_socket_bind(const int, const struct addrinfo* const);

extern int  net_socket_connect(const int, const struct addrinfo* const);

extern int  net_socket_setopt_true(const int, const int, const int);

extern int  net_socket_setopt_false(const int, const int, const int);

extern void net_socket_reuse_addr(const int);

extern void net_socket_dont_reuse_addr(const int);

extern void net_socket_reuse_port(const int);

extern void net_socket_dont_reuse_port(const int);

extern int  net_socket_get_family(const int);

extern int  net_socket_get_socktype(const int);

extern int  net_socket_get_protocol(const int);

extern void net_socket_get_peer_address(const int, void* const);

extern void net_socket_get_local_address(const int, void* const);

extern void net_socket_dont_block(const int);

extern void net_socket_default_options(const int);

#endif // MHNJj_yfLA3WP__Eq_f4M__J_JwdkH_i