#ifndef cVo_XMDq_D3_6_0UO_aHsaP__CXtCo_1
#define cVo_XMDq_D3_6_0UO_aHsaP__CXtCo_1 1

#include "tcp.h"

#include <openssl/ssl.h>

/* Try to mimic TCP as much as possible when creating TLS */

enum tls_consts {
  /* FLAGS */
  
  tls_wants_send = 1073741824,      /* 1 << 30 */
  tls_opened = 536870912,           /* 1 << 29 */
  tls_onreadclose_once = 268435456, /* 1 << 28 */
  tls_shutdown_wr = 134217728,      /* 1 << 27 */
  
  /* SETTINGS */
  
  tls_onreadclose_callback = 0U,
  tls_onreadclose_tls_close = 1U,
  tls_onreadclose_tcp_close = 2U,
  tls_onreadclose_tcp_force_close = 3U,
  tls_onreadclose_do_nothing = 4U
};

struct tls_socket;

struct tls_socket_callbacks {
  int (*oncreation)(struct tls_socket*);
  
  void (*onopen)(struct tls_socket*);
  
  void (*onmessage)(struct tls_socket*);
  /*
  There is a possibility that onclose() will be called without onopen() with TLS.
  It may happen when the TLS handshake failed. onclose() will then be called to
  free the socket. This is the same as if TCP received an error after connect().
  
  The onsend() callback is useless with TLS, and so it is not defined. TLS buffers
  records by itself. This also implies that TCP setting disable_send_buffer MUST
  always be disabled (so that send_buffer is enabled). */
  
  void (*tcp_onreadclose)(struct tls_socket*);
  
  void (*tls_onreadclose)(struct tls_socket*);
  
  int (*onnomem)(struct tls_socket*);
  
  void (*onclose)(struct tls_socket*);
  
  void (*onfree)(struct tls_socket*);
};

struct tls_socket_settings {
  unsigned read_buffer_cleanup_threshold;
  unsigned read_buffer_growth;
  unsigned force_close_on_fatal_error:1;
  unsigned force_close_on_shutdown_error:1;
  unsigned force_close_tcp:1;
  unsigned onreadclose_auto_res:3;
};

struct tls_record {
  int size;
  int total_size;
  // ... data
};

struct tls_socket {
  struct tcp_socket tcp;
  
  struct tls_socket_callbacks* callbacks;
  struct tls_socket_settings* settings;
  SSL_CTX* ctx; /* This needs to be provided by the application */
  SSL* ssl;
  pthread_mutex_t read_lock;
  pthread_mutex_t ssl_lock;
  char* read_buffer;
  unsigned read_used;
  unsigned read_size;
};

extern void tls_socket_free(struct tls_socket* const);

extern void tls_socket_close(struct tls_socket* const);

extern void tls_socket_force_close(struct tls_socket* const);


extern int tls_socket_init(struct tls_socket* const, const int);

extern int tls_create_socket(struct tls_socket* const);

extern int tls_send(struct tls_socket* const, const void*, int);

extern int tls_read(struct tls_socket* const, void* const, const int);

extern unsigned char tls_peek(const struct tls_socket* const, const int);

extern unsigned char tls_peek_once(struct tls_socket* const, const int);



struct tls_server;

struct tls_server_callbacks {
  /*
  This callback is called when a TCP connection arrives, not a TLS one. The
  application must then fill the following information of the socket:
  - socket->tcp.settings
  - socket->callbacks
  - socket->settings
  - socket->ssl and other SSL members by simply using tls_socket_init()
  Note that socket->ctx will by default be set to its server's ctx. It may be
  changed by the application though.
  The new socket's epoll will be set to its server's epoll by default, as it is
  required. It MUST NOT be changed.
  As with TCP, the callback is also used to validate the incoming connection.
  The application should return net_success to let it pass, or net_failure to
  destroy it. */
  int (*onconnection)(struct tls_socket*);
  
  int (*onnomem)(struct tls_server*);
  
  void (*onshutdown)(struct tls_server*);
};

struct tls_server {
  struct tcp_server tcp;
  
  struct tls_server_callbacks* callbacks;
  SSL_CTX* ctx; /* This needs to be provided by the application */
};


extern void tls_server_free(struct tls_server* const);

extern int tls_create_server(struct tls_server* const, struct addrinfo* const);

extern void tls_server_foreach_conn(struct tls_server* const, void (*)(struct tls_socket*, void*), void*, const int);

extern void tls_server_dont_accept_conn(struct tls_server* const);

extern void tls_server_accept_conn(struct tls_server* const);

extern int tls_server_shutdown(struct tls_server* const);

extern unsigned tls_server_get_conn_amount(struct tls_server* const);



extern int tls_epoll(struct net_epoll* const);

extern void tls_ignore_sigpipe(void);

extern void tls_get_OpenSSL_error(char* const, const size_t);

#endif // cVo_XMDq_D3_6_0UO_aHsaP__CXtCo_1