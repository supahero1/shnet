#ifndef _shnet_net_h_
#define _shnet_net_h_ 1

#ifdef __cplusplus
extern "C" {
#endif

#include <netdb.h>
#include <stdint.h>
#include <sys/un.h>
#include <sys/types.h>
#include <sys/socket.h>


enum net_consts
{
	/* FAMILIES */
	NET_FAMILY_ANY = AF_UNSPEC,
	NET_FAMILY_IPV4 = AF_INET,
	NET_FAMILY_IPV6 = AF_INET6,
	NET_FAMILY_UNIX = AF_UNIX,

	/* SOCKTYPES */
	NET_SOCK_ANY = 0,
	NET_SOCK_STREAM = SOCK_STREAM,
	NET_SOCK_DATAGRAM = SOCK_DGRAM,
	NET_SOCK_RAW = SOCK_RAW,
	NET_SOCK_SEQPACKET = SOCK_SEQPACKET,

	/* PROTOCOLS */
	NET_PROTO_ANY = 0,
	NET_PROTO_TCP = IPPROTO_TCP,
	NET_PROTO_UDP = IPPROTO_UDP,
	NET_PROTO_UDP_LITE = IPPROTO_UDPLITE,

	/* FLAGS */
	NET_FLAG_WANTS_SERVER = AI_PASSIVE,
	NET_FLAG_WANTS_CANONICAL_NAME = AI_CANONNAME,
	NET_FLAG_NUMERIC_HOSTNAME = AI_NUMERICHOST,
	NET_FLAG_NUMERIC_SERVICE = AI_NUMERICSERV,
	NET_FLAG_WANTS_OWN_IP_VERSION = AI_ADDRCONFIG,
	NET_FLAG_WANTS_ALL_ADDRESSES = AI_ALL,
	NET_FLAG_WANTS_MAPPED_IPV4 = AI_V4MAPPED,

	/* CONSTANTS */

	NET_CONST_IPV4_STRLEN = 16,
	NET_CONST_IPV6_STRLEN = 46,
	NET_CONST_UNIX_STRLEN = 109,
	NET_CONST_IP_MAX_STRLEN = NET_CONST_UNIX_STRLEN,

	NET_CONST_IPV4_SIZE = sizeof(struct sockaddr_in),
	NET_CONST_IPV6_SIZE = sizeof(struct sockaddr_in6),
	NET_CONST_UNIX_SIZE = sizeof(struct sockaddr_un),
	NET_CONST_ADDR_MAX_SIZE = sizeof(struct sockaddr_storage)
};


struct net_async_address;


typedef struct sockaddr_storage net_address_t;

typedef sa_family_t net_family_t;

typedef void (*net_async_t)(struct net_async_address*, struct addrinfo*);


struct net_async_address
{
	char* hostname;
	char* port;

	struct addrinfo* hints;

	void* data;
	net_async_t callback;
};



extern struct addrinfo
net_get_addr_struct(int family, int socktype, int protocol, int flags);


extern struct addrinfo*
net_get_address_sync(const char* hostname,
	const char* port, const struct addrinfo* hints);


extern int
net_get_address_async(struct net_async_address* addr);


extern void
net_free_address(struct addrinfo* info);



extern void*
net_get_address(const struct addrinfo* info);


extern void
net_address_to_string(const void* addr, char* buffer);


extern net_family_t
net_address_to_family(const void* addr);


extern uint16_t
net_address_to_port(const void* addr);


extern void*
net_address_to_ip(const void* addr);



extern int
net_socket_get(const struct addrinfo* hints);


extern int
net_socket_bind(int sfd, const struct addrinfo* info);


extern int
net_socket_listen(int sfd, int backlog);


extern int
net_socket_connect(int sfd, const struct addrinfo* info);


extern int
net_socket_accept(int sfd);


extern int
net_socket_setopt_true(int sfd, int level, int option_name);


extern int
net_socket_setopt_false(int sfd, int level, int option_name);


extern void
net_socket_reuse_addr(int sfd);


extern void
net_socket_dont_reuse_addr(int sfd);


extern void
net_socket_reuse_port(int sfd);


extern void
net_socket_dont_reuse_port(int sfd);


extern net_family_t
net_socket_get_family(int sfd);


extern int
net_socket_get_socktype(int sfd);


extern int
net_socket_get_protocol(int sfd);


extern void
net_socket_get_peer_address(int sfd, void* addr);


extern void
net_socket_get_local_address(int sfd, void* addr);


extern void
net_socket_dont_block(int sfd);


extern void
net_socket_default_options(int sfd);


#ifdef __cplusplus
}
#endif

#endif /* _shnet_net_h_ */
