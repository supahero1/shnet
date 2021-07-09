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
  /* For this to work, before the last corked send the application must call
  tcp_socket_cock_off(), NOT AFTER. This way, we don't switch to kernel twice.
  That is because this option is NOT created using setsockopt(). */
  tcp_cork = 16U,
  
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
  When a new server socket is created and the oncreation() callback is called, it
  is also the responsibility of the application to fill in required members of the 
  socket, that is:
  - socket->callbacks
  - socket->settings
  Only then can it return successfully. Otherwise it is undefined behavior and will
  likely lead to an instant crash.
  The new socket's epoll will automatically be set to its server's epoll. It is
  undefined behavior if the application changes it to a different epoll or if it
  will tamper with the socket's server pointer.
  Returning -1 will result in the socket being destroyed.
  onfree() can be called after oncreation() if the socket failed at any further
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
  unsigned send_buffer_cleanup_threshold;
  unsigned onreadclose_auto_res:1;
  unsigned remove_from_epoll_onclose:1;
};

/* A tcp_socket's address after the call to tcp_create_socket() MUST NOT BE CHANGED.
It is undefined behavior otherwise. That is also true for tcp_server. */

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
  _Atomic uint32_t flags;
};

extern void tcp_socket_cork_on(struct tcp_socket* const);

extern void tcp_socket_cork_off(struct tcp_socket* const);

extern int tcp_socket_keepalive(const struct tcp_socket* const);

extern int tcp_socket_keepalive_explicit(const struct tcp_socket* const, const int, const int, const int);

extern void tcp_socket_free(struct tcp_socket* const);

extern void tcp_socket_close(struct tcp_socket* const);

extern void tcp_socket_force_close(struct tcp_socket* const);

extern int tcp_create_socket(struct tcp_socket* const);

extern int tcp_send(struct tcp_socket* const, const void*, int);

extern int tcp_read(struct tcp_socket* const, void*, int);



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
  unsigned max_conn;
  int backlog;
  /* The application must fill this in if it plans to embed a socket's struct
  inside its own struct. For servers, it will signal to allocate more memory.
  Otherwise, leave it at 0 for the underlying code to set to the default. */
  unsigned socket_size;
};

struct tcp_server {
  struct net_socket_base base;
  struct tcp_server_callbacks* callbacks;
  struct tcp_server_settings* settings;
  struct net_epoll* epoll;
  pthread_rwlock_t lock; /* Not a mutex to let the application inspect sockets */
  /* If the application has some memory to spare, it can set the 2 members below.
  Memory for sockets must be settings->socket_size * settings->max_connections,
  and for freeidx it must be sizeof(unsigned) * settings->max_connections. Sockets
  memory must be zeroed, freeidx doesn't need to be.
  If any error occurs during tcp_create_server(), the memory will be freed.
  The application does not need to initialise these. If it doesn't, they will be
  initialised automatically. */
  char* sockets;
  unsigned* freeidx;
  unsigned freeidx_used;
  unsigned sockets_used;
  unsigned disallow_connections:1;
  unsigned is_closing:1;
};

extern void tcp_server_free(struct tcp_server* const);

extern int tcp_create_server(struct tcp_server* const);

extern void tcp_server_foreach_conn(struct tcp_server* const, void (*)(struct tcp_socket*, void*), void*, const int);

extern void tcp_server_dont_accept_conn(struct tcp_server* const);

extern void tcp_server_accept_conn(struct tcp_server* const);

extern int tcp_server_shutdown(struct tcp_server* const);

extern unsigned tcp_server_get_conn_amount_raw(const struct tcp_server* const);

extern unsigned tcp_server_get_conn_amount(struct tcp_server* const);



extern int tcp_epoll(struct net_epoll* const);

#endif // __Qz_51_Jfgr_mbwOPa_oh_vMG78RXim