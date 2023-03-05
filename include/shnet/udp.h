#ifndef _shnet_udp_h_
#define _shnet_udp_h_ 1

#ifdef __cplusplus
extern "C" {
#endif

#include <shnet/net.h>
#include <shnet/async.h>


enum udp_type
{
	UDP_CLIENT,
	UDP_SERVER
};


struct udp_socket;


typedef void (*udp_event_t)(struct udp_socket*);


struct udp_socket
{
	int fd;

	uint8_t alloc_loop:1;
	uint8_t udp_lite:1;
	uint8_t type:1;

	udp_event_t on_event;

	struct async_loop* loop;
};


extern int
udp_client(struct udp_socket* client, const struct addrinfo* info);


extern int
udp_server(struct udp_socket* server, const struct addrinfo* info);


extern void
udp_send(const struct udp_socket* socket, const void* data, size_t len);


extern ssize_t
udp_read(const struct udp_socket* socket, void* out,
	size_t out_len, struct addrinfo* out_info);


extern void
udp_free(struct udp_socket* socket);


extern void
udp_onevent(struct async_loop*, int*, uint32_t);


extern int
udp_async_loop(struct async_loop* loop);


#ifdef __cplusplus
}
#endif

#endif /* _shnet_udp_h_ */
