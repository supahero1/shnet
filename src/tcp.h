#ifndef __Qz_51_Jfgr_mbwOPa_oh_vMG78RXim
#define __Qz_51_Jfgr_mbwOPa_oh_vMG78RXim 1

#include "net.h"
#include "misc.h"

/* The rules of TCP (and TLS):
1. A socket or a server can ONLY be in 1 epoll at a time
2. All server sockets must be in the same epoll as their server
3. A socket or a server may only be used from 1 thread at a time (excluding epoll)
*/

enum tcp_consts {
  /* TCP socket flags */
  
  tcp_shutdown_wr = 1U,
  tcp_can_send = 2U,
  tcp_opened = 4U,
  tcp_data_ended = 8U,
  
  /* TCP server flags */
  
  tcp_disallow_connections = 1U,
  tcp_server_closing = 2U
};

struct tcp_socket;

/* All callbacks besides onclose() and onnomem() are optional and can be NULL */
struct tcp_socket_callbacks {
  /* This callback is called when the socket is created, no matter if it's a
  client or a server socket. It is meaningful, because the application might want
  to initialise something too, by embeding the socket's structure in it's own.
  The application must return 0 on success and -1 on failure.
  A failure has an additional meaning with server sockets - the application can
  inspect them before it decides to let them in. If such a socket is deemed invalid
  by the application, it shall return -1 and not initialise anything.
  When a new server socket is created and the onconnection() callback is called, it
  is also the responsibility of the application to fill in required members of the
  socket, that is:
  - socket->callbacks
  - socket->settings (can be left empty, will then use the default)
  Only then can it return successfully. Otherwise it is undefined behavior and will
  likely lead to an instant crash.
  The new socket's epoll will automatically be set to its server's epoll. It is
  undefined behavior if the application changes it to a different epoll or if it
  will tamper with the socket's server pointer.
  Returning -1 will result in the socket being destroyed.
  onfree() can be called after onconnection() if the socket failed at any further
  stage of a server socket's creation. It will also be called as part of tcp_socket_free().
  onclose() may be called without onopen() if connection fails to be established.
  Setting this callback to anything other than NULL is meaningless if done as part
  of onconnection().
  If the callback fails while initialising the socket, it must cleanup after itself.
  Only cleanup what you have done in the function and not the whole socket. Example:
  1. Init a mutex
  2. Init another mutex, but the call fails
  3. Instead of returning instantly, first destroy the first mutex
  4. Return -1 */
  int (*oncreation)(struct tcp_socket*);
  /* The socket can only be accessed after onopen() callback is fired. The application
  may start sending data inside of the onopen() callback. */
  void (*onopen)(struct tcp_socket*);
  /* The onmessage() callback doesn't mean the application needs to read any data.
  It only informs that data is available. It is up to the application to call tcp_read(). */
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
  /* We are out of memory. Either free some and return 0, or halt, or close the socket
  and return -1. */
  int (*onnomem)(struct tcp_socket*);
  /* The underlying protocol has been closed. There still might be data pending,
  so it is not forbidden to call tcp_read(), however tcp_send() can't be called.
  After or inside of the onclose() callback, the application must find appropriate
  time to call the tcp_socket_free() function, which will proceed to free the socket.
  That also means the application should only call it when it is sure it will no
  longer access the socket in any way. */
  void (*onclose)(struct tcp_socket*);
  /* Called when the socket is freed. For instance, if the application allocated
  any resources for the socket when onconnection() was called, the application can
  also free these resources in this function whenever the socket is freed.
  If the freed socket is in a server, the application MUST zero whole area of the
  socket besides the base tcp_socket members. Otherwise, the application must zero
  anything that is not a constant, i.e. settings or callbacks, that are not part of
  a tcp_socket. The tcp_socket part will be cleaned by the underlying code. */
  void (*onfree)(struct tcp_socket*);
};

struct tcp_socket_settings {
  uint64_t send_buffer_cleanup_threshold;
  uint32_t onreadclose_auto_res:1;
  uint32_t remove_from_epoll_onclose:1;
  uint32_t dont_free_addrinfo:1;
  /* Only available for non-server sockets. Applications which allocate their
  sockets dynamically will need to use this function so that they don't need to
  override the TCP's tcp_socket_free() function to free the socket. That's because
  callbacks are always called from the top-most layer to the bottom-most, at the
  bottom being TCP. Freeing the socket at the top-most layer would create a bunch
  of invalid reads and writes, causing segmentation fault. */
  uint32_t free_on_free:1;
  /* Look tcp_server's "offset" member of settings for an explanation. */
  uint32_t free_offset:28;
  uint32_t _unused:32;
};

/* A tcp_socket's address after the call to tcp_create_socket() MUST NOT BE CHANGED.
It is undefined behavior otherwise. That is also true for tcp_server. */

struct tcp_socket {
  struct net_socket_base base;
  struct tcp_server* server;
  struct tcp_socket_callbacks* callbacks;
  struct tcp_socket_settings* settings;
  struct net_epoll* epoll;
  /* If the connection fails and info is not NULL, the connection will be silently
  retried with the next available address */
  struct addrinfo* info;
  struct addrinfo* cur_info;
  pthread_mutex_t lock;
  char* send_buffer;
  uint64_t send_used;
  uint64_t send_size;
  _Atomic uint32_t flags;
};

extern void tcp_socket_cork_on(struct tcp_socket* const);

extern void tcp_socket_cork_off(struct tcp_socket* const);

extern void tcp_socket_nodelay_on(struct tcp_socket* const);

extern void tcp_socket_nodelay_off(struct tcp_socket* const);

extern int  tcp_socket_keepalive(const struct tcp_socket* const);

extern int  tcp_socket_keepalive_explicit(const struct tcp_socket* const, const int, const int, const int);

extern void tcp_socket_linger(const struct tcp_socket* const, const int);

extern void tcp_socket_stop_receiving_data(struct tcp_socket* const);


extern void tcp_socket_free(struct tcp_socket* const);

extern void tcp_socket_close(struct tcp_socket* const);

extern void tcp_socket_force_close(struct tcp_socket* const);

extern int  tcp_create_socket(struct tcp_socket* const);

extern uint64_t tcp_send(struct tcp_socket* const, const void*, uint64_t);

extern uint64_t tcp_read(struct tcp_socket* const, void*, uint64_t);



struct tcp_server;

struct tcp_server_callbacks {
  /* You can read more about this callback above at the oncreation() socket callback */
  int (*onconnection)(struct tcp_socket*);
  /* Inside of the epoll thread(s), we could run out of memory while accepting
  a new connection. Some applications prefer to halt the program, others have
  a mechanism for releasing some memory. Let the application choose the approach.
  If the application fails to release memory, it should return -1.
  Otherwise, if it attempted to do something, it should return 0.
  This function might be called multiple times in a row, in case the application
  freed not enough memory. */
  int (*onnomem)(struct tcp_server*);
  /* Called when the server completed its shutdown. It is no more in any epoll,
  it has no more connections. The array of sockets might not be zeroed. */
  void (*onshutdown)(struct tcp_server*);
};

struct tcp_server_settings {
  uint32_t max_conn;
  int backlog;
  /* Offset of the created socket. Only to be used by applications which embed
  tcp_socket inside their own structure somewhere else than the beginning. Note
  that after doing this, one needs to also change the socket's pointer when
  in callback to be able to access the original struct the socket is embedded in.
  The offset is a positive integer in bytes saying how far from the beginning the
  tcp_socket lies. */
  uint32_t offset;
};

struct tcp_server {
  struct net_socket_base base;
  struct tcp_server_callbacks* callbacks;
  struct tcp_server_settings* settings;
  struct net_epoll* epoll;
  pthread_rwlock_t lock; /* Not a mutex to let the application inspect sockets */
  /* If the application has some memory to spare, it can set the 2 members below.
  Memory for sockets must be settings->socket_size * settings->max_connections,
  and for freeidx it must be sizeof(uint32_t) * settings->max_connections. Sockets
  memory must be zeroed, freeidx doesn't need to be.
  If any error occurs during tcp_create_server(), the memory will be freed.
  The application does not need to initialise these. If it doesn't, they will be
  initialised automatically. */
  char* sockets;
  uint32_t* freeidx;
  uint32_t freeidx_used;
  uint32_t sockets_used;
  /* The application must fill this in if it plans to embed a socket's struct
  inside its own struct. For servers, it will signal to allocate more memory.
  Otherwise, leave it at 0 for the underlying code to set to the default. */
  uint32_t socket_size;
  uint32_t disallow_connections:1;
  uint32_t is_closing:1;
};

extern void tcp_server_free(struct tcp_server* const);

extern int  tcp_create_server(struct tcp_server* const, struct addrinfo* const);

extern void tcp_server_foreach_conn(struct tcp_server* const, void (*)(struct tcp_socket*, void*), void*, const int);

extern void tcp_server_dont_accept_conn(struct tcp_server* const);

extern void tcp_server_accept_conn(struct tcp_server* const);

extern int  tcp_server_shutdown(struct tcp_server* const);

extern uint32_t tcp_server_get_conn_amount_raw(const struct tcp_server* const);

extern uint32_t tcp_server_get_conn_amount(struct tcp_server* const);



extern int  tcp_epoll(struct net_epoll* const);

#endif // __Qz_51_Jfgr_mbwOPa_oh_vMG78RXim