#ifndef __Qz_51_Jfgr_mbwOPa_oh_vMG78RXim
#define __Qz_51_Jfgr_mbwOPa_oh_vMG78RXim 1

#include "net.h"
#include "misc.h"

#include <openssl/ssl.h>
#include <openssl/bio.h>

/*
All TCP sockets must be used with an epoll instance.
They must be non-blocking.
thats also to not create stuff for 5789347895934 cases cuz that would take AGES
*/

enum tcp_consts {
  tcp_socket = 0U,
  tcp_server = 1U,
  
  /* TCP socket state */
  tcp_closed = 0U,
  tcp_connecting = 1U,
  tcp_connected = 2U,
  tcp_closing = 3U,
  
  /* TCP socket flags */
  tcp_can_close = 1U,
  tcp_internal_shutdown_wr = 2U,
  tcp_internal_shutdown_rd = 4U,
  tcp_can_send = 8U,
  tcp_can_read = 16U,
  tcp_wants_send = 32U,
  tcp_wants_read = 64U,
  
  tcp_fatal = 0,
  tcp_proceed
};

#define TCP_INTERNAL_STATE_BIT_SIZE 3U
#define TCP_INTERNAL_STATE_BITSHIFT 2U

struct tcp_storage {
  struct net_socket_base base;
  int which;
};

struct tcp_socket;

struct tcp_socket_callbacks {
  /* Socket can only be accessed after onopen() handler is fired */
  void (*onopen)(struct tcp_socket*);
  /* The onmessage() handler doesn't mean the application needs to read any data.
  It only informs that data is available. It is up to the application to call
  tcp_read(). */
  void (*onmessage)(struct tcp_socket*);
  /* If in the onclose() handler you call tcp_socket_confirm_close(), NEVER
  keep using the socket afterwards. If the application isn't sure if the socket
  will be accessed, it should NOT call tcp_socket_confirm_close(). */
  void (*onclose)(struct tcp_socket*);
  int (*onerror)(struct tcp_socket*);
};

struct tcp_socket_settings {
  unsigned send_buffer_cleanup_threshold;
  unsigned send_buffer_allow_freeing:1;
};

struct tcp_socket {
  struct net_socket_base base;
  int which;
  struct tcp_server* server;
  struct tcp_socket_callbacks* callbacks;
  struct tcp_socket_settings* settings;
  struct net_epoll* epoll;
  pthread_rwlock_t lock;
  SSL_CTX* ctx;
	SSL* ssl;
	BIO* read_bio;
  BIO* send_bio;
  char* send_buffer;
  unsigned send_used;
  unsigned send_size;
  unsigned send_total_size;
  _Atomic unsigned state;
  int close_reason;
};

extern int tcp_is_fatal(const int);

extern void tcp_socket_close(struct tcp_socket* const);

extern void tcp_socket_confirm_close(struct tcp_socket* const);

extern void tcp_socket_confirm_force_close(struct tcp_socket* const);

extern int tcp_create_socket_base(struct tcp_socket* const);

extern int tcp_socket_tls(struct tcp_socket* const);

extern int tcp_create_socket(struct tcp_socket* const);

extern int tcp_send(struct tcp_socket* const, const void*, int);

extern int tcp_handler_send(struct tcp_socket* const, const void*, int);

extern int tcp_read(struct tcp_socket* const, void* const, const int, int* const);

extern int tcp_handler_read(struct tcp_socket* const, void* const, const int, int* const);



extern void tcp_onevent(struct net_epoll*, int, void*);






























// WORK IN PROGRESS

struct tcp_settings {
  unsigned long max_connections;
};

struct tcp_server {
  struct net_socket_base base;
  int which;
  struct contmem mem;
  pthread_rwlock_t lock;
  struct tcp_settings* settings;
  void (*onconnection)(struct tcp_server*, struct tcp_socket*);
  void (*onerror)(struct tcp_server*);
};










#endif // __Qz_51_Jfgr_mbwOPa_oh_vMG78RXim