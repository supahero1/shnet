#ifndef _shnet_tcp_h_
#define _shnet_tcp_h_ 1

#ifdef __cplusplus
extern "C" {
#endif

#include <shnet/net.h>
#include <shnet/async.h>
#include <shnet/storage.h>


enum tcp_event
{
	TCP_OPEN,
	TCP_DATA,
	TCP_CAN_SEND,
	TCP_READCLOSE,
	TCP_CLOSE,
	TCP_DEINIT,
	TCP_FREE
};


enum tcp_type
{
	TCP_CLIENT,
	TCP_SERVER
};


struct tcp_socket;


typedef struct tcp_socket* (*tcp_event_t)(struct tcp_socket*, enum tcp_event);


struct tcp_socket
{
	int fd;

	uint8_t type:1;
	uint8_t alloc_loop:1;
	uint8_t opened:1;
	uint8_t confirmed_free:1;
	uint8_t closing:1;
	uint8_t close_guard:1;
	uint8_t closing_fast:1;
	uint8_t free_ptr:1;
	/* TLS Extensions */
	uint8_t alloc_ctx:1;
	uint8_t alloc_ssl:1;
	uint8_t tls_close_guard:1;
	uint8_t init_fin:1;
	uint8_t shutdown_once:1;

	pthread_mutex_t lock;
	struct data_storage queue;

	tcp_event_t on_event;

	struct async_loop* loop;
};


struct tcp_options
{
	struct addrinfo* info;
	const char* hostname;
	const char* port;

	int family;
	int flags;

	int backlog;
};


extern void
tcp_lock(struct tcp_socket* socket);


extern void
tcp_unlock(struct tcp_socket* socket);


extern uint16_t
tcp_get_local_port(const struct tcp_socket* server);


extern void
tcp_cork_on(const struct tcp_socket* client);


extern void
tcp_cork_off(const struct tcp_socket* client);


extern void
tcp_nodelay_on(const struct tcp_socket* client);


extern void
tcp_nodelay_off(const struct tcp_socket* client);


extern void
tcp_keepalive_on_explicit(const struct tcp_socket* client,
	int idle_time, int reprobe_time, int retries);


extern void
tcp_keepalive_on(const struct tcp_socket* client);


extern void
tcp_keepalive_off(const struct tcp_socket* client);


extern void
tcp_free_common(struct tcp_socket* socket);


extern void
tcp_free(struct tcp_socket* socket);


extern void
tcp_close(struct tcp_socket* socket);


extern void
tcp_terminate(struct tcp_socket* client);


extern int
tcp_send_buffered(struct tcp_socket* client);


extern int
tcp_send(struct tcp_socket* client, const struct data_frame* frame);


extern uint64_t
tcp_read(struct tcp_socket* client, void* out, uint64_t out_len);


extern uint64_t
tcp_send_len(const struct tcp_socket* client);


extern uint64_t
tcp_recv_len(const struct tcp_socket* socket);


extern struct tcp_socket*
tcp_get_server(const struct tcp_socket* client);


extern int
tcp_client(struct tcp_socket* client, const struct tcp_options* options);


extern int
tcp_server(struct tcp_socket* server, const struct tcp_options* options);


extern void
tcp_onevent(struct async_loop*, int*, uint32_t);


extern int
tcp_async_loop(struct async_loop* loop);


#ifdef __cplusplus
}
#endif

#endif /* _shnet_tcp_h_ */
