#ifndef cVo_XMDq_D3_6_0UO_aHsaP__CXtCo_1
#define cVo_XMDq_D3_6_0UO_aHsaP__CXtCo_1 1

#include "tcp.h"

#include <openssl/ssl.h>

/* Try to mimic TCP as much as possible when creating TLS */

enum tls_consts {
  /* TLS socket flags */
  
  tls_wants_send = 32U,
  tls_shutdown_wr = 64U,
  tls_shutdown_rd = 128U,
  
  tls_read_only = tcp_read_only,
  tls_dont_free = tcp_dont_free,
  
  tls_client = 1,
  tls_rsa_key = 2
};

struct tls_socket;

struct tls_socket_callbacks {
  int  (*oncreation)(struct tls_socket*);
  void (*onopen)(struct tls_socket*);
  void (*onmessage)(struct tls_socket*);
  int  (*onnomem)(struct tls_socket*);
  void (*onclose)(struct tls_socket*);
  void (*onfree)(struct tls_socket*);
};

struct tls_socket_settings {
  /* Look tcp.h for explanation */
  uint8_t automatically_reconnect:1;
  uint8_t onopen_when_reconnect:1;
  uint8_t oncreation_when_reconnect:1;
  /* onclose_when_reconnect needs to be set in the TCP settings */
  uint8_t init:1;
};

struct tls_socket {
  struct tcp_socket tcp;
  pthread_mutex_t read_lock;
  pthread_mutex_t ssl_lock;
  uint64_t read_used;
  uint64_t read_size;
  uint64_t read_growth;
  
  const struct tls_socket_callbacks* callbacks;
  SSL_CTX* ctx;
  SSL* ssl;
  char* read_buffer;
  struct tls_socket_settings settings;
  uint8_t clean:1;
  uint8_t alloc_ctx:1;
  uint8_t alloc_ssl:1;
  uint8_t opened:1;
  uint8_t close_once:1;
};

extern void tls_socket_free(struct tls_socket* const);

extern void tls_socket_close(struct tls_socket* const);

extern void tls_socket_force_close(struct tls_socket* const);

extern void tls_socket_dont_receive_data(struct tls_socket* const);

extern void tls_socket_receive_data(struct tls_socket* const);


extern SSL_CTX* tls_ctx(const char* const, const char* const, const uintptr_t);

extern int  tls_socket_init(struct tls_socket* const);

struct tls_socket_options {
  struct tcp_socket_options tcp;
  const char* cert_path;
  const char* key_path;
  uintptr_t flags;
};

extern int  tls_socket(struct tls_socket* const, const struct tls_socket_options* const);

extern int  tls_send(struct tls_socket* const, const void*, uint64_t, const int);

extern uint64_t tls_read(struct tls_socket* const, void* const, const uint64_t);

extern unsigned char tls_peek(const struct tls_socket* const, const uint64_t);

extern unsigned char tls_peek_once(struct tls_socket* const, const uint64_t);



struct tls_server;

struct tls_server_callbacks {
  /*
  This callback is called when a TCP connection arrives, not a TLS one. The
  application must then fill the following information of the socket:
  - socket->tcp.settings (optional)
  - socket->callbacks
  - socket->settings (optional)
  Note that socket->ctx will by default be set to its server's ctx. It may be
  changed by the application though.
  The new socket's epoll will be set to its server's epoll by default, as it is
  required. It MUST NOT be changed.
  As with TCP, the callback is also used to validate the incoming connection.
  The application should return 0 to let it pass, or -1 to drop it. */
  int  (*onconnection)(struct tls_socket*, const struct sockaddr*);
  int  (*onnomem)(struct tls_server*);
  void (*onerror)(struct tls_server*);
  void (*onshutdown)(struct tls_server*);
};

struct tls_server {
  struct tcp_server tcp;
  
  const struct tls_server_callbacks* callbacks;
  SSL_CTX* ctx;
};


extern void tls_server_free(struct tls_server* const);

struct tls_server_options {
  struct tcp_server_options tcp;
  const char* cert_path;
  const char* key_path;
  uintptr_t flags;
};

extern int  tls_server(struct tls_server* const, struct tls_server_options* const);

extern void tls_server_foreach_conn(struct tls_server* const, void (*)(struct tls_socket*, void*), void*);

extern void tls_server_dont_accept_conn(struct tls_server* const);

extern void tls_server_accept_conn(struct tls_server* const);

extern int  tls_server_shutdown(struct tls_server* const);

extern uint32_t tls_server_get_conn_amount_raw(const struct tls_server* const);

extern uint32_t tls_server_get_conn_amount(struct tls_server* const);


extern int  tls_epoll(struct net_epoll* const);

extern void tls_ignore_sigpipe(void);

extern void tls_get_OpenSSL_error(char* const, const size_t);

#endif // cVo_XMDq_D3_6_0UO_aHsaP__CXtCo_1