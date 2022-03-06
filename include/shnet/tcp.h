#ifndef __Qz_51_Jfgr_mbwOPa_oh_vMG78RXim
#define __Qz_51_Jfgr_mbwOPa_oh_vMG78RXim 1

#include <shnet/net.h>
#include <shnet/async.h>
#include <shnet/storage.h>

enum tcp_consts {
  tcp_shutdown_wr = 1U << 0,
  tcp_send_available = 1U << 1,
  tcp_closing = 1U << 2,
  tcp_closing_fast = 1U << 3,
  tcp_confirmed_free = 1U << 4,
  
  tcp_consts_len = 5
};

enum tcp_event {
  tcp_open,
  tcp_data,
  tcp_can_send,
  tcp_readclose,
  tcp_close,
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
  uint8_t free:1;
  uint8_t close_guard:1;
  uint8_t dont_send_buffered:1;
  uint8_t dont_close_onreadclose:1;
  uint8_t dont_autoclean:1;
  /* Extensions */
  uint8_t alloc_ctx:1;
};

extern void tcp_lock(struct tcp_socket* const);

extern void tcp_unlock(struct tcp_socket* const);

extern void tcp_socket_cork_on(struct tcp_socket* const);

extern void tcp_socket_cork_off(struct tcp_socket* const);

extern void tcp_socket_nodelay_on(struct tcp_socket* const);

extern void tcp_socket_nodelay_off(struct tcp_socket* const);

extern void tcp_socket_keepalive_explicit(const struct tcp_socket* const, const int, const int, const int);

extern void tcp_socket_keepalive(const struct tcp_socket* const);

extern void tcp_socket_dont_receive_data(struct tcp_socket* const);

extern int  tcp_socket_unread_data(const struct tcp_socket* const);

extern int  tcp_socket_unsent_data(const struct tcp_socket* const);

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
  
  struct tcp_socket* (*on_event)(struct tcp_server*, enum tcp_event, struct tcp_socket*);
  struct async_loop* loop;
  
  uint8_t alloc_loop:1;
  uint8_t alloc_info:1;
  /* Extensions */
  uint8_t alloc_ctx:1;
};

extern uint16_t tcp_server_get_port(const struct tcp_server* const);

extern void tcp_server_free(struct tcp_server* const);

extern int  tcp_server_close(struct tcp_server* const);

struct tcp_server_options {
  struct addrinfo* info;
  const char* hostname;
  const char* port;
  int family;
  int flags;
  int backlog;
};

extern int  tcp_server(struct tcp_server* const, const struct tcp_server_options* const);


extern int  tcp_async_loop(struct async_loop* const);

#endif // __Qz_51_Jfgr_mbwOPa_oh_vMG78RXim