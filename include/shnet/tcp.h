#ifndef _shnet_tcp_h_
#define _shnet_tcp_h_ 1

#ifdef __cplusplus
extern "C" {
#endif

#include <shnet/net.h>
#include <shnet/async.h>
#include <shnet/storage.h>

enum tcp_event {
  tcp_open,
  tcp_data,
  tcp_can_send,
  tcp_readclose,
  tcp_close,
  tcp_deinit,
  tcp_free
};

struct tcp_socket {
  struct async_event core;
  pthread_mutex_t lock;
  
  void (*on_event)(struct tcp_socket*, enum tcp_event);
  struct async_loop* loop;
  
  struct data_storage queue;
  uint8_t alloc_loop:1;
  uint8_t opened:1;
  uint8_t confirmed_free:1;
  uint8_t closing:1;
  uint8_t close_guard:1;
  uint8_t closing_fast:1;
  uint8_t free:1;
  uint8_t dont_send_buffered:1;
  uint8_t dont_close_onreadclose:1;
  uint8_t dont_autoclean:1;
  /* TLS Extensions */
  uint8_t alloc_ctx:1;
  uint8_t alloc_ssl:1;
  uint8_t tls_close_guard:1;
  uint8_t init_fin:1;
  uint8_t shutdown_once:1;
};

extern void tcp_lock(struct tcp_socket* const);

extern void tcp_unlock(struct tcp_socket* const);

extern void tcp_socket_cork_on(const struct tcp_socket* const);

extern void tcp_socket_cork_off(const struct tcp_socket* const);

extern void tcp_socket_nodelay_on(const struct tcp_socket* const);

extern void tcp_socket_nodelay_off(const struct tcp_socket* const);

extern void tcp_socket_keepalive_on_explicit(const struct tcp_socket* const, const int, const int, const int);

extern void tcp_socket_keepalive_on(const struct tcp_socket* const);

extern void tcp_socket_keepalive_off(const struct tcp_socket* const);

extern void tcp_socket_free_(struct tcp_socket* const);

extern void tcp_socket_free(struct tcp_socket* const);

extern void tcp_socket_close(struct tcp_socket* const);

extern void tcp_socket_force_close(struct tcp_socket* const);

struct tcp_socket_options {
  struct addrinfo* info;
  const char* hostname;
  const char* port;
  int family;
  int flags;
};

extern int  tcp_socket(struct tcp_socket* const, const struct tcp_socket_options* const);

extern int  tcp_send(struct tcp_socket* const, const struct data_frame* const);

extern uint64_t tcp_read(struct tcp_socket* const, void*, uint64_t);


struct tcp_server {
  struct async_event core;
  
  struct tcp_socket* (*on_event)(struct tcp_server*, struct tcp_socket*, enum tcp_event);
  struct async_loop* loop;
  
  uint8_t alloc_loop:1;
  /* TLS Extensions */
  uint8_t alloc_ctx:1;
};

extern uint16_t tcp_server_get_port(const struct tcp_server* const);

extern void tcp_server_free(struct tcp_server* const);

extern void tcp_server_close(struct tcp_server* const);

struct tcp_server_options {
  struct addrinfo* info;
  const char* hostname;
  const char* port;
  int family;
  int flags;
  int backlog;
};

extern int  tcp_server(struct tcp_server* const, const struct tcp_server_options* const);


extern void tcp_onevent(struct async_loop*, uint32_t, struct async_event*);

extern int  tcp_async_loop(struct async_loop* const);

#ifdef __cplusplus
}
#endif

#endif // _shnet_tcp_h_
