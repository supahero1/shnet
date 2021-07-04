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
  
  /* SETTINGS */
  
  tls_onreadclose_callback = 0U,
  tls_onreadclose_tls_close = 1U,
  tls_onreadclose_tcp_close = 2U,
  tls_onreadclose_tcp_force_close = 3U
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
  always be disabled (so that send_buffer is enabled).
  
  The onnomem callback was originally used with TCP servers to signal that there
  was no memory for a new socket. With TLS sockets, there are a lot of places all
  across the code which require memory. To not make the application check return
  value for a lot of different functions and to make out of memory error handling
  way easier, which might potentially increase stability and throughput, this
  callback has been created.
  In 2 places in particular, the application must either free some memory or close
  the socket. Thus, it should always assume that and call tcp_socket_close() if
  it fails to fulfill the library's demands. In that case, it should still return
  net_failure though, to peacefully stop whatever operation was in progress. */
  
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
  unsigned onreadclose_auto_res:2;
};

struct tls_record {
  int size;
  int total_size;
  // ... data
};

struct tls_socket {
  /* This is a copy of tcp_socket's members. Better this than creating a new
  member that we will need to access all the time, increasing code size and
  reducing readability. */
  struct net_socket_base base;
  struct tls_server* server;
  struct tcp_socket_callbacks* callbacks;
  struct tcp_socket_settings* settings;
  struct net_epoll* epoll;
  pthread_mutex_t lock;
  char* send_buffer;
  unsigned send_used;
  unsigned send_size;
  _Atomic uint32_t flags;
  
  struct tls_socket_callbacks* tls_callbacks;
  struct tls_socket_settings* tls_settings;
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


extern int tls_oncreation(struct tcp_socket*);

extern void tls_onopen(struct tcp_socket*);

extern void tls_onmessage(struct tcp_socket*);

extern void tls_onreadclose(struct tcp_socket*);

extern void tls_onsend(struct tcp_socket*);

extern int tls_socket_onnomem(struct tcp_socket*);

extern void tls_onclose(struct tcp_socket*);

extern void tls_onfree(struct tcp_socket*);


extern int tls_socket_init(struct tls_socket* const, const int);

extern int tls_create_socket(struct tls_socket* const);

extern int tls_send(struct tls_socket* const, const void*, int);

extern int tls_read(struct tls_socket* const, void* const, const int);

extern unsigned char tls_peak(const struct tls_socket* const, const int);

extern unsigned char tls_peak_once(struct tls_socket* const, const int);

#define tls_default_tcp_socket_callbacks (struct tcp_socket_callbacks) \
{tls_oncreation,tls_onopen,tls_onmessage,tls_onreadclose,tls_onsend,tls_socket_onnomem,tls_onclose,tls_onfree}



/* A TLS server is extremely similar to a TCP one, to the point where there are
no new settings, almost no new struct tls_server members, almost all functions
call the TCP version of themselves, and so on. */

struct tls_server;

struct tls_server_callbacks {
  /*
  This callback is called when a TCP connection arrives, not a TLS one. The
  application must then fill the following information of the socket:
  - socket->callbacks
  - socket->settings
  - socket->tls_callbacks
  - socket->tls_settings
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
  struct net_socket_base base;
  struct tcp_server_callbacks* callbacks;
  struct tcp_server_settings* settings;
  struct net_epoll* epoll;
  pthread_rwlock_t lock;
  char* sockets;
  unsigned* freeidx;
  unsigned freeidx_used;
  unsigned sockets_used;
  unsigned disallow_connections:1;
  unsigned is_closing:1;
  
  struct tls_server_callbacks* tls_callbacks;
  SSL_CTX* ctx; /* This needs to be provided by the application */
};


extern int tls_onconnection(struct tcp_socket*);

extern int tls_server_onnomem(struct tcp_server*);

extern void tls_onshutdown(struct tcp_server*);


extern void tls_server_free(struct tls_server* const);

extern int tls_create_server(struct tls_server* const);

extern void tls_server_foreach_conn(struct tls_server* const, void (*)(struct tls_socket*, void*), void*, const int);

extern void tls_server_dont_accept_conn(struct tls_server* const);

extern void tls_server_accept_conn(struct tls_server* const);

extern int tls_server_shutdown(struct tls_server* const);

extern unsigned tls_server_get_conn_amount(struct tls_server* const);

#define tls_default_tcp_server_callbacks (struct tcp_server_callbacks) \
{tls_onconnection,tls_server_onnomem,tls_onshutdown}



extern int tls_epoll(struct net_epoll* const);

extern void tls_ignore_sigpipe(void);

extern void tls_get_OpenSSL_error(char* const, const size_t);

#endif // cVo_XMDq_D3_6_0UO_aHsaP__CXtCo_1