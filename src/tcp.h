#ifndef __Qz_51_Jfgr_mbwOPa_oh_vMG78RXim
#define __Qz_51_Jfgr_mbwOPa_oh_vMG78RXim 1

#include "net.h"

enum tcp_consts {
  /* TCP socket flags */
  
  tcp_shutdown_wr = 1U,
  tcp_can_send = 2U,
  tcp_opened = 4U,
  tcp_closing = 8U,
  
  tcp_read_only = 1,
  tcp_dont_free
};

struct tcp_socket;

/* All callbacks besides onclose() and onnomem() are optional and can be NULL */
struct tcp_socket_callbacks {
  /* This callback is called when the socket is created. It is meaningful,
  because the application might want to initialise something too, by embedding
  the socket's structure in it's own. The application must return 0 on success
  and -1 on failure. If the callback fails while initialising the socket, it
  must cleanup after itself. Only cleanup what you have done in the function and
  not the whole socket. For example:
  1. Init a mutex
  2. Init another mutex, but the call fails
  3. Instead of returning instantly, first destroy the first mutex
  4. Return -1 */
  int  (*oncreation)(struct tcp_socket*);
  /* The socket can only be accessed after onopen() callback is fired. The
  application may start sending data inside of the onopen() callback. */
  void (*onopen)(struct tcp_socket*);
  /* The onmessage() callback doesn't mean the application needs to read any
  data. It merely informs that data is available. It is up to the application to
  call tcp_read(). */
  void (*onmessage)(struct tcp_socket*);
  /* onreadclose() merely means that the peer has finished sending data to us.
  The application can keep calling tcp_read() until errno EPIPE. The application
  must choose an appropriate time to close the connection by calling
  tcp_socket_close(). Usually, the most appropriate time is once the application
  is done sending all data. With some protocols, it might be a good idea to
  close the socket as soon as this occurs. This is possible without setting the
  callback using the automatically_close_onreadclose socket option below. In
  that case, this callback can be NULL. */
  void (*onreadclose)(struct tcp_socket*);
  /* This callback is called when it is possible to send more data. In other
  words, data from the kernel's send buffer has been acknowledged by the peer */
  void (*onsend)(struct tcp_socket*);
  /* We are out of memory. Either free some and return 0, or halt, or close the
  socket and return -1. Note that the socket won't be closed on its own if -1 is
  returned. If you don't know what to do, tcp_socket_close() the socket when
  returning -1. Not closing the socket is undefined behavior, but it may be
  delayed until later - the application must at least remove it from its epoll.
  If you don't know how to implement this function:
  1. Always return -1 and half the program / close the socket,
  2. Attempt to free resources that your program allocated. For instance, if you
  allocated anything ahead-of-time, or allocated area of memory that might not
  be fully used, try realloc()-ing these memory areas to free up some memory.
  WARNING: NEVER do this repetively. The application should employ some kind of
  a global counter of out-of-memory calls, resetting it when requesting memory
  succeeds. When the counter is increased above a certain limit, start returning
  -1. This way, no infinite loop is possible, but if not resetting the counter
  sufficient amount of times, it could lead to false positives. */
  int  (*onnomem)(struct tcp_socket*);
  /* The underlying protocol has been closed. There still might be data pending,
  so it is not forbidden to call tcp_read(), however tcp_send() can't be called.
  After or inside of the onclose() callback, the application must find
  appropriate time to call the tcp_socket_free() function, which will proceed to
  free the socket. That also means the application should only call it when it
  is sure it will no longer access the socket in any way. If the application
  doesn't free the socket inside of the onclose() callback, the socket's file
  descriptor will be removed from its epoll automatically by the underlying code
  after onclose().
  The onclose() callback may be called without onopen() if the connection failed
  at any further point than tcp_socket(). */
  void (*onclose)(struct tcp_socket*);
  /* Called when the socket is freed. For instance, if the application allocated
  any resources for the socket when oncreation() or onconnection() was called,
  the application can free these resources in this function. */
  void (*onfree)(struct tcp_socket*);
};

/* All settings besides automatically_close_onreadclose for server sockets
either are meaningless, or MUST NOT be changed by the application */
struct tcp_socket_settings {
  /* Automatically close the connection once the peer closes their channel */
  uint8_t automatically_close_onreadclose:1;
  /* Applications which allocate their sockets dynamically will need to use this
  function so that they don't need to override the TCP's tcp_socket_free()
  function to free the socket. That's because callbacks are always called from
  the top-most layer to the bottom-most, at the bottom being TCP. Freeing the
  socket at the top-most layer would create a bunch of invalid reads and writes,
  causing a segmentation fault. */
  uint8_t free_on_free:1;
  /* If the connection is closed AFTER successfully connecting, i.e. onopen()
  was called, and if this setting is set, the underlying code will automatically
  reuse the socket and reconnect to the same address.
  If the address is found invalid, and no more addresses are available, the
  underlying code will fetch a fresh list of addresses to the host. If none of
  the addresses succeed, the onclose() callback will finally be called. */
  uint8_t automatically_reconnect:1;
  /* This setting modifies behavior of the above. If it's set, onopen() will be
  called when reconnecting. Useful if the application does some kind of a
  handshake upon connecting. */
  uint8_t onopen_when_reconnect:1;
  /* The same as above, but calls onclose() */
  uint8_t onclose_when_reconnect:1;
  /* Self explanatory */
  uint8_t oncreation_when_reconnect:1;
  /* Must always be set to 1 */
  uint8_t init:1;
  /* For use when buffered bytes aren't the ones we want to send, used for
  instance with TLS. Leave it at 0 for TCP, but 1 for TLS. Remember that this
  option doesn't work for tcp_send(), it only restricts epoll. */
  uint8_t dont_send_buffered:1;
};//TODO when sending and we fail somewhere, check if the data has dont_free, and if it doesnt, fucking free it111111

struct tcp_socket_send_frame {
  const char* data;
  uint64_t offset:63;
  /* If read_only is not set, data points to an allocated region of memory that
  will be freed after being sent, unless dont_free is set. Otherwise, the data
  region is meant to be read-only. This allows sending a lot of data that
  remains the same - very useful for static file hosting or such. */
  uint64_t read_only:1;
  uint64_t len:63;
  uint64_t dont_free:1;
};

struct tcp_address {
  const char* hostname;
  const char* port;
};

struct tcp_socket {
  struct net_socket net;
  pthread_mutex_t lock;
  
  struct tcp_server* server;
  const struct tcp_socket_callbacks* callbacks;
  struct net_epoll* epoll;
  struct addrinfo* info;
  struct addrinfo* cur_info;
  struct tcp_address* addr;
  struct tcp_socket_send_frame* send_queue;
  
  uint32_t send_len;
  /* Please never change any settings after adding a socket to an epoll. They
  are not declared as const like callbacks, because they are not a pointer. */
  struct tcp_socket_settings settings;
  _Atomic uint8_t flags;
  uint8_t alloc_epoll:1;
  uint8_t alloc_addr:1;
  uint8_t reprobed:1;
  uint8_t reconnecting:1;
};

extern void tcp_socket_cork_on(struct tcp_socket* const);

extern void tcp_socket_cork_off(struct tcp_socket* const);

extern void tcp_socket_nodelay_on(struct tcp_socket* const);

extern void tcp_socket_nodelay_off(struct tcp_socket* const);

extern int  tcp_socket_keepalive(const struct tcp_socket* const);

extern int  tcp_socket_keepalive_explicit(const struct tcp_socket* const, const int, const int, const int);

extern void tcp_socket_force_close(struct tcp_socket* const);

extern void tcp_socket_dont_receive_data(struct tcp_socket* const);


extern void tcp_socket_free(struct tcp_socket* const);

extern void tcp_socket_close(struct tcp_socket* const);

extern void tcp_socket_force_close(struct tcp_socket* const);

struct tcp_socket_options {
  const char* hostname;
  const char* port;
  int family;
  int flags;
};

extern int  tcp_socket(struct tcp_socket* const, const struct tcp_socket_options* const);

extern int  tcp_buffer(struct tcp_socket* const, const void*, uint64_t, uint64_t, const int);

extern int  tcp_send(struct tcp_socket* const, const void*, uint64_t, const int);

extern uint64_t tcp_read(struct tcp_socket* const, void*, uint64_t);



struct tcp_server;

struct tcp_server_callbacks {
  /* This callback is called each time a new socket is successfully accepted.
  The application is bound to fill callbacks of the newly created socket. It may
  also set the socket's settings, but it is optional. The socket's epoll will be
  set to the server's epoll, and it MUST NOT be changed by the application. The
  application may allocate resources for the socket in this callback, but
  setting the onfree() callback is then necessary to be able to free the
  resources later. If settings won't be provided, default TCP socket settings
  will be used (not malloc()-ed). Treat this callback like tcp_socket's
  oncreation(). The application can use this callback as a method of filtering
  incoming connections. Returning -1 will result in the socket being closed
  right away. On success, the application should return 0. */
  int (*onconnection)(struct tcp_socket*, const struct sockaddr*);
  /* Look tcp_socket's onnomem() for details. */
  int (*onnomem)(struct tcp_server*);
  /* Called when a socket couldn't be initialised. The application might want to
  know about possible resource shortages, like lack of file descriptors. This
  callback is optional. */
  void (*onerror)(struct tcp_server*);
  /* Called when the server completed its shutdown. It is no more in any epoll,
  it has no more connections. The array of sockets might not be zeroed. It can
  then be freed. */
  void (*onshutdown)(struct tcp_server*);
};

struct tcp_server_settings {
  /* Self explanatory */
  uint32_t max_conn;
  int backlog;
};

struct tcp_server {
  struct net_server net;
  const struct tcp_server_callbacks* callbacks;
  const struct tcp_server_settings* settings;
  struct net_epoll* epoll;
  /* If the application has some memory to spare, it can set the 2 members
  below. The sizes must be at least:
  calloc(settings->max_conn, settings->socket_size);
  malloc(sizeof(uint32_t) * settings->max_conn);
  respectively.
  The application does not need to initialise these. If allocated by the
  application, they will never be freed by the underlying code. */
  char* sockets;
  uint32_t* freeidx;
  
  pthread_mutex_t lock;
  
  uint32_t sockets_used;
  uint32_t freeidx_used;
  /* The application must fill this in if it plans to embed a socket's struct
  inside its own struct. For servers, it will signal to allocate more memory.
  Otherwise, leave it at 0 for the underlying code to set to the default. */
  uint32_t socket_size;
  _Atomic uint8_t flags;
  uint8_t disallow_connections:1;
  uint8_t alloc_sockets:1;
  uint8_t alloc_freeidx:1;
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

extern void tcp_server_dont_accept_conn(struct tcp_server* const);

extern void tcp_server_accept_conn(struct tcp_server* const);

extern int  tcp_server_shutdown(struct tcp_server* const);

extern uint32_t tcp_server_get_conn_amount_raw(const struct tcp_server* const);

extern uint32_t tcp_server_get_conn_amount(struct tcp_server* const);


extern int  tcp_epoll(struct net_epoll* const);

#endif // __Qz_51_Jfgr_mbwOPa_oh_vMG78RXim