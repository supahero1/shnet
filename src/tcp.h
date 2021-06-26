#ifndef __Qz_51_Jfgr_mbwOPa_oh_vMG78RXim
#define __Qz_51_Jfgr_mbwOPa_oh_vMG78RXim 1

#include "net.h"
#include "misc.h"

/* The rules of TCP:
1. A socket or a server can ONLY be in 1 epoll at a time
2. All server sockets must be in the same epoll as their server
*/

enum tcp_consts {
  /* TCP socket state */
  tcp_closed = 0U,
  tcp_connected = 1U,
  
  /* TCP socket flags */
  tcp_shutdown_wr = 2U,
  tcp_can_send = 4U,
  tcp_opened = 8U,
  tcp_data_ended = 16U,
  /* For this to work, before the last corked send the application must call
  tcp_socket_cock_off(), NOT AFTER. This way, we don't switch to kernel twice.
  That is because this option is NOT created using setsockopt(). */
  tcp_cork = 32U,
  
  /* TCP server flags */
  tcp_disallow_connections = 1U,
  tcp_server_closing = 2U,
    
  tcp_fatal = 0,
  tcp_proceed
};

struct tcp_socket;

/* All callbacks besides onclose() are optional and can be NULL. */
struct tcp_socket_callbacks {
  /* The socket can only be accessed after onopen() callback is fired */
  void (*onopen)(struct tcp_socket*);
  /* The onmessage() callback doesn't mean the application needs to read any data.
  It only informs that data is available. It is up to the application to call
  tcp_read(). */
  void (*onmessage)(struct tcp_socket*);
  /* onreadclose() merely means that the peer has finished sending data to us.
  The application can keep calling tcp_read() until errno EPIPE. The application
  must choose an appropriate time to close the connection by calling tcp_socket_close().
  Usually, the most appropriate time is once the application is done sending all data. */
  void (*onreadclose)(struct tcp_socket*);
  /* This callback was originally created for applications which use disable_send_buffer
  setting and have their own system of buffering data to be sent, but now it is
  called regardless of the setting. Some applications might want to know right away
  when they can send more data, for instance to perform a benchmark. */
  void (*onsend)(struct tcp_socket*);
  /* The underlying protocol has been closed. There still might be data pending,
  so it is not forbidden to call tcp_read(), however tcp_send() can't be called.
  After or inside of the onclose() callback, the application must find appropriate
  time to call the tcp_socket_confirm_close() function, which will proceed to free
  the socket. That also means the application should only call it when it is sure
  it will no longer access the socket in any way. */
  void (*onclose)(struct tcp_socket*);
};

struct tcp_socket_settings {
  unsigned send_buffer_cleanup_threshold;
  unsigned send_buffer_allow_freeing:1;
  unsigned disable_send_buffer:1;
  unsigned onreadclose_auto_res:1;
};

/* A tcp_socket's address after the call to tcp_create_socket() MUST NOT BE CHANGED.
It is undefined behavior otherwise. */

struct tcp_socket {
  struct net_socket_base base;
  struct tcp_server* server;
  struct tcp_socket_callbacks* callbacks;
  struct tcp_socket_settings* settings;
  struct net_epoll* epoll;
  pthread_mutex_t lock;
  char* send_buffer;
  unsigned send_used;
  unsigned send_size;
  int close_reason;
  _Atomic uint32_t state;
};

extern int tcp_socket_get_close_reason(const struct tcp_socket* const);

extern void tcp_socket_cork_on(struct tcp_socket* const);

extern void tcp_socket_cork_off(struct tcp_socket* const);

extern int tcp_socket_keepalive(const struct tcp_socket* const);

extern int tcp_socket_keepalive_explicit(const struct tcp_socket* const, const int, const int, const int);

extern void tcp_socket_close(const struct tcp_socket* const);

extern void tcp_socket_confirm_close(struct tcp_socket* const);

extern int tcp_socket_force_close(struct tcp_socket* const);

extern int tcp_create_socket_base(struct tcp_socket* const);

extern int tcp_create_socket(struct tcp_socket* const);

extern int tcp_send(struct tcp_socket* const, const void*, int);

extern int tcp_read(struct tcp_socket* const, void*, int);



struct tcp_server;

struct tcp_server_callbacks {
  /* This callback is called when a new connection has been acknowledged by the
  server. The application MUST fill the following information of the socket:
  - socket->epoll
  - socket->callbacks
  - socket->settings
  The rest will be done by the library. The application should return tcp_proceed.
  Otherwise, if the application has some sort of a filter or a firewall to drop
  connections, it can do it from within this function. The socket's file descriptor
  and its address will be set. If the application decides to drop the connection,
  it MUST return tcp_fatal. The rest will be done by the library.
  WARNING: the socket might still be terminated. It MUST NOT be assumed that the
  socket will not fail after this function, especially when using TLS.
  All sockets MUST belong to the same epoll the server is in. It is done automatically
  by the underlying code. */
  int (*onconnection)(struct tcp_server*, struct tcp_socket*);
  /* If after onconnection() we destroyed the connection for whatever reason, and
  the application allocated some memory for it in onconnection(), it can release
  that memory in this callback. It can be NULL. */
  void (*ontermination)(struct tcp_server*, struct tcp_socket*);
  /* Inside of the epoll thread(s), we could run out of memory while accepting
  a new connection. Some applications prefer to halt the program, others have
  a mechanism for releasing some memory. Let the application choose the approach.
  If the application fails to release memory, it should return tcp_fatal.
  Otherwise, if it attempted to do something, it should return tcp_proceed.
  This function might be called multiple times in a row, in case the application
  freed not enough memory. */
  int (*onnomem)(struct tcp_server*);
  /* Called when the server completed its shutdown */
  void (*onshutdown)(struct tcp_server*);
};

struct tcp_server_settings {
  unsigned max_conn;
  int backlog;
};

struct tcp_server {
  struct net_socket_base base;
  struct tcp_server_callbacks* callbacks;
  struct tcp_server_settings* settings;
  struct net_epoll* epoll;
  pthread_rwlock_t lock; /* Not a mutex to let the application inspect sockets */
  /* If the application has some memory to spare, it can set the 2 members below.
  Memory for sockets must be sizeof(struct tcp_socket) * settings->max_connections,
  and for freeidx it must be sizeof(unsigned) * settings->max_connections. Sockets
  memory must be zeroed, freeidx doesn't need to be.
  If any error occurs during tcp_create_server(), the memory will be freed.
  The application does not need to initialise these. If it doesn't, they will be
  initialised automatically. */
  struct tcp_socket* sockets;
  unsigned* freeidx;
  unsigned freeidx_used;
  unsigned sockets_used;
  _Atomic unsigned connections;
  _Atomic uint32_t flags;
};

extern void tcp_server_free(struct tcp_server* const);

extern int tcp_create_server_base(struct tcp_server* const);

extern int tcp_create_server(struct tcp_server* const);

extern void tcp_server_foreach_conn(struct tcp_server* const, void (*)(struct tcp_socket*, void*), void*, const int);

extern void tcp_server_dont_accept_conn(struct tcp_server* const);

extern void tcp_server_accept_conn(struct tcp_server* const);

extern int tcp_server_shutdown(struct tcp_server* const);

extern unsigned tcp_server_get_conn_amount(const struct tcp_server* const);



extern int tcp_epoll(struct net_epoll* const);

#endif // __Qz_51_Jfgr_mbwOPa_oh_vMG78RXim