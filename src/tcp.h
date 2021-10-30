#ifndef __Qz_51_Jfgr_mbwOPa_oh_vMG78RXim
#define __Qz_51_Jfgr_mbwOPa_oh_vMG78RXim 1

#include "net.h"
#include "storage.h"

enum tcp_consts {
  tcp_shutdown_wr = 1U,
  tcp_send_available = 2U,
  tcp_closing = 4U
};

enum tcp_event {
  tcp_creation,
  tcp_open,
  tcp_data,
  tcp_can_send,
  tcp_readclose,
  tcp_close,
  tcp_destruction,
  tcp_free
};

struct tcp_socket_settings {
  uint8_t automatically_close_onreadclose:1;
  uint8_t automatically_reconnect:1;
  uint8_t onopen_when_reconnect:1;
  uint8_t onclose_when_reconnect:1;
  /* Implies destructor will be called as well */
  uint8_t oncreation_when_reconnect:1;
  /* Must always be set to 1 */
  uint8_t init:1;
  /* For use when buffered bytes aren't the ones we want to send, used for
  instance with TLS. Leave it at 0 for TCP, 1 for TLS. This option doesn't work
  for tcp_send(), it only restricts epoll. */
  uint8_t dont_send_buffered:1;
};

struct tcp_address {
  const char* hostname;
  const char* port;
};

struct tcp_socket {
  struct net_socket net;
  /* Usually, there is no lock contention. It may only arise unfortunately when
  both epoll and the application are trying to send something at the same time,
  or when auto reconnecting is on and when a reconnection occurs, the
  application is trying to close the socket. */
  pthread_mutex_t lock;
  
  int (*on_event)(struct tcp_socket*, enum tcp_event);
  struct tcp_server* server;
  
  struct data_storage queue;
  /* Settings MUST NOT be modified during normal socket operation. If it really
  is necessary, remove it from its epoll or stop the epoll to make sure the
  settings won't be accessed when changing them.
  Settings aren't const-qualified, because server sockets must have their
  settings initialised after creation. That requires them to be non-const. */
  struct tcp_socket_settings settings;
  _Atomic uint8_t flags;
  uint8_t alloc_epoll:1;
  uint8_t alloc_info:1;
  uint8_t alloc_addr:1;
  uint8_t opened:1;
  uint8_t reprobed:1;
  uint8_t reconnecting:1;
  uint8_t confirm_free:1;
  
  struct net_epoll* epoll;
  /* If the addrinfo is user-generated and the only pointer to it is given to
  the socket, IT MIGHT BE LOST! Set alloc_addr to 1 if that's the case to
  prevent such memory leak. If you are using it for multiple sockets, NEVER set
  alloc_addr to 1 and make sure to keep a backup pointer to it that will be
  freed later. The information stands only for info member below. */
  struct addrinfo* info;
  struct addrinfo* cur_info;
  struct tcp_address* addr;
};

extern void tcp_socket_cork_on(struct tcp_socket* const);

extern void tcp_socket_cork_off(struct tcp_socket* const);

extern void tcp_socket_nodelay_on(struct tcp_socket* const);

extern void tcp_socket_nodelay_off(struct tcp_socket* const);

extern void tcp_socket_keepalive(const struct tcp_socket* const);

extern void tcp_socket_keepalive_explicit(const struct tcp_socket* const, const int, const int, const int);

extern void tcp_socket_force_close(struct tcp_socket* const);

extern void tcp_socket_dont_receive_data(struct tcp_socket* const);


extern void tcp_socket_free(struct tcp_socket* const);

extern void tcp_socket_close(struct tcp_socket* const);

extern void tcp_socket_force_close(struct tcp_socket* const);

struct tcp_socket_options {
  const char* hostname;
  const char* port;
  int32_t family:30;
  /* It's static if the pointer won't be deallocated or changed and can be used
  by the library */
  uint32_t static_hostname:1;
  uint32_t static_port:1;
  int flags;
};

extern int  tcp_socket(struct tcp_socket* const, const struct tcp_socket_options* const);

extern int  tcp_buffer(struct tcp_socket* const, void* const, const uint64_t, const uint64_t, const enum data_storage_flags);

extern int  tcp_send(struct tcp_socket* const, void*, uint64_t, const enum data_storage_flags);

extern uint64_t tcp_read(struct tcp_socket* const, void*, uint64_t);



struct tcp_serversock {
  struct net_socket net;
  pthread_mutex_t lock;
  
  int (*on_event)(struct tcp_socket*, enum tcp_event);
  struct tcp_server* server;
  
  struct data_storage queue;
  struct tcp_socket_settings settings;
  _Atomic uint8_t flags;
  uint8_t alloc_epoll:1;
  uint8_t alloc_info:1;
  uint8_t alloc_addr:1;
  uint8_t opened:1;
  uint8_t reprobed:1;
  uint8_t reconnecting:1;
  uint8_t confirm_free:1;
};

struct tcp_server_settings {
  uint32_t max_conn;
  /* Size of the TCP listen queue */
  int backlog;
};

struct tcp_server {
  struct net_socket net;
  pthread_mutex_t lock;
  
  int (*on_event)(struct tcp_server*, enum tcp_event, struct tcp_socket*, const struct sockaddr*);
  const struct tcp_server_settings* settings;
  struct net_epoll* epoll;
  /* If the application has some memory to spare, it can set the member below.
  The size must be at least:
  calloc(settings->max_conn, settings->socket_size);
  The application does not need to initialise it. If allocated by the
  application, it will never be freed by the underlying code. To allow
  automatic freeing, set alloc_sockets to 1. The same is true for epoll.
  Note: sockets MUST NOT HAVE THEIR POINTER CHANGED! */
  char* sockets;
  
  uint32_t sockets_used;
  uint32_t freeidx;
  uint32_t sockets_len;
  /* The application must fill this in if it plans to embed a socket's struct
  inside its own struct. For servers, it will signal to allocate more memory.
  Otherwise, leave it at 0 for the underlying code to set to the default.
  socket_size MUST be at least sizeof(struct tcp_serversock). That way, no spare
  memory is allocated. Otherwise, the spare memory is located behind the tcp
  socket (the tcp socket is first in order). */
  uint32_t socket_size;
  _Atomic uint8_t flags;
  uint8_t disallow_connections:1;
  uint8_t alloc_sockets:1;
  uint8_t alloc_epoll:1;
  /* Extension for TLS */
  uint8_t alloc_ctx:1;
};

extern void tcp_server_free(struct tcp_server* const);

struct tcp_server_options {
  struct addrinfo* info;
  const char* hostname;
  const char* port;
  int family;
  int flags;
};

extern int  tcp_server(struct tcp_server* const, const struct tcp_server_options* const);

extern void tcp_server_foreach_conn(struct tcp_server* const, void (*)(struct tcp_socket*, void*), void*);

extern void tcp_server_dont_accept_conn_raw(struct tcp_server* const);

extern void tcp_server_dont_accept_conn(struct tcp_server* const);

extern void tcp_server_accept_conn_raw(struct tcp_server* const);

extern void tcp_server_accept_conn(struct tcp_server* const);

extern int  tcp_server_shutdown(struct tcp_server* const);

extern uint32_t tcp_server_get_conn_amount_raw(const struct tcp_server* const);

extern uint32_t tcp_server_get_conn_amount(struct tcp_server* const);


extern int  tcp_socket_epoll(struct net_epoll* const);

extern int  tcp_server_epoll(struct net_epoll* const);

#endif // __Qz_51_Jfgr_mbwOPa_oh_vMG78RXim