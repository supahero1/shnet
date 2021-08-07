#ifndef yt8_UOxpTCa__YeX9iyxAAG3U0J_HwVs
#define yt8_UOxpTCa__YeX9iyxAAG3U0J_HwVs 1

#include "time.h"
#include "tls.h"
#include "http_p.h"
#include "compress.h"
#include "hash_table.h"

#include <stdint.h>

enum http_consts {
  ws_read_only = tcp_read_only,
  ws_dont_free = tcp_dont_free,
  ws_not_last_frame = 4
};

enum http_close_codes {
  http_unreachable,
  http_invalid_response,
  http_no_keepalive,
  http_unexpected_close,
  http_timeouted,
  http_no_memory
};

extern const char* http_str_close_reason(const int);

struct http_uri {
  char* hostname;
  char* path;
  char* port_ptr;
  uint32_t hostname_len;
  uint32_t path_len;
  uint16_t port;
  uint8_t port_len;
  uint8_t secure:1;
};

struct http_request {
  struct http_message* request;
  struct http_parser_settings* response_settings;
  /* Compression */
  uint8_t quality;
  uint8_t window_bits;
  uint8_t mem_level;
  uint8_t mode;
  
  uint8_t no_cache:1;
  uint8_t compression_isnt_required:1;
};

struct http_callbacks;
struct https_callbacks;

struct http_options {
  uint64_t timeout_after;
  uint64_t read_growth;
  
  struct http_request* requests;
  struct time_manager* manager;
  struct net_epoll* epoll;
  struct addrinfo* info;
  const struct http_callbacks* http_callbacks;
  const struct https_callbacks* https_callbacks;
  SSL_CTX* ctx;
  
  uint32_t requests_len;
  int family;
  int flags;
};

struct http_requests {
  uint64_t len;
  char* msg;
  struct http_parser_settings* response_settings;
};

struct http_context {
  uint64_t timeout_after;
  uint64_t read_growth;
  uint64_t read_used;
  uint64_t read_size;
  uint32_t requests_used;
  uint32_t requests_size;
  struct http_parser_session session;
  struct http_message response;
  struct http_header headers[255];
  
  struct http_requests* requests;
  struct time_manager* manager;
  struct time_reference timeout;
  char* read_buffer;
  
  uint8_t alloc_manager:1;
  uint8_t expected:1;
};


struct http_socket;

struct http_callbacks {
  void (*onresponse)(struct http_socket*, struct http_message*);
  int  (*onnomem)(struct http_socket*);
  void (*onclose)(struct http_socket*, int);
};

struct http_socket {
  struct tcp_socket tcp;
  struct http_context context;
  const struct http_callbacks* callbacks;
};


struct https_socket;

struct https_callbacks {
  void (*onresponse)(struct https_socket*, struct http_message*);
  int  (*onnomem)(struct https_socket*);
  void (*onclose)(struct https_socket*, int);
};

struct https_socket {
  struct tls_socket tls;
  struct http_context context;
  const struct https_callbacks* callbacks;
};

extern int  http(char* const, struct http_options*);



struct http_serversock;
struct https_serversock;

struct ws_serversock_callbacks {
  void (*onmessage)(struct http_serversock*, void*, uint64_t);
  void (*onreadclose)(struct http_serversock*, int);
  void (*onclose)(struct http_serversock*, int);
};

struct wss_serversock_callbacks {
  void (*onmessage)(struct https_serversock*, void*, uint64_t);
  void (*onreadclose)(struct https_serversock*, int);
  void (*onclose)(struct https_serversock*, int);
};

struct http_serversock_context {
  uint64_t read_used;
  uint64_t read_size;
  
  struct http_parser_session session;
  struct http_parser_settings backup_settings;
  struct http_message message;
  struct http_header headers[255];
  
  struct http_parser_settings* settings;
  struct http_hash_table_entry* entry;
  struct time_reference timeout;
  z_stream* deflater;
  pthread_mutex_t* deflater_mutex;
  z_stream* inflater;
  char* read_buffer;
  
  uint8_t expected:1;
  uint8_t protocol:1;
  uint8_t permessage_deflate:1;
  uint8_t closing:1;
  uint8_t close_onreadclose:1;
  uint8_t init_parsed:1;
};

struct http_serversock {
  struct tcp_socket tcp;
  struct http_serversock_context context;
  const struct ws_serversock_callbacks* callbacks;
};

struct https_serversock {
  struct tls_socket tls;
  struct http_serversock_context context;
  const struct wss_serversock_callbacks* callbacks;
};


struct http_server;
struct https_server;

struct http_resource {
  char* path;
  struct http_parser_settings* settings;
  union {
    void (*http_callback)(struct http_server*, struct http_serversock*, struct http_message*, struct http_message*);
    void (*https_callback)(struct https_server*, struct https_serversock*, struct http_message*, struct http_message*);
  };
};

struct http_server_callbacks;
struct https_server_callbacks;

struct http_server_options {
  uint64_t read_growth;
  uint64_t timeout_after;
  uint32_t resources_len;
  uint32_t socket_size;
  
  struct http_resource* resources;
  const struct tcp_server_settings* tcp_settings;
  struct http_parser_settings* default_settings;
  struct time_manager* manager;
  struct net_epoll* epoll;
  struct addrinfo* info;
  SSL_CTX* ctx;
  const struct http_server_callbacks* http_callbacks;
  const struct https_server_callbacks* https_callbacks;
  struct http_server* http_server;
  struct https_server* https_server;
  struct http_hash_table* table;
  
  int family;
  int flags;
};

struct http_server_context {
  uint64_t read_growth;
  uint64_t timeout_after;
  
  struct time_manager* manager;
  struct http_parser_settings* default_settings;
  struct http_hash_table* table;
  /* HTTP compression */
  uint8_t quality;
  uint8_t window_bits;
  uint8_t mem_level;
  uint8_t mode;
  uint8_t compression_isnt_required:1;
  
  uint8_t alloc_table:1;
  uint8_t alloc_manager:1;
};


struct http_server;

struct http_server_callbacks {
  int  (*onnomem)(struct http_server*);
  void (*onerror)(struct http_server*);
  void (*onshutdown)(struct http_server*);
};

struct http_server {
  struct tcp_server tcp;
  struct http_server_context context;
  const struct http_server_callbacks* callbacks;
};


struct https_server;

struct https_server_callbacks {
  int  (*onnomem)(struct https_server*);
  void (*onerror)(struct https_server*);
  void (*onshutdown)(struct https_server*);
};

struct https_server {
  struct tls_server tls;
  struct http_server_context context;
  const struct https_server_callbacks* callbacks;
};

extern int  http_server(char* const, struct http_server_options*);


extern void http_server_foreach_conn(struct http_server* const, void (*)(struct http_serversock*, void*), void*);

extern void http_server_dont_accept_conn(struct http_server* const);

extern void http_server_accept_conn(struct http_server* const);

extern int  http_server_shutdown(struct http_server* const);

extern uint32_t http_server_get_conn_amount_raw(const struct http_server* const);

extern uint32_t http_server_get_conn_amount(struct http_server* const);


extern void https_server_foreach_conn(struct https_server* const, void (*)(struct https_serversock*, void*), void*);

extern void https_server_dont_accept_conn(struct https_server* const);

extern void https_server_accept_conn(struct https_server* const);

extern int  https_server_shutdown(struct https_server* const);

extern uint32_t https_server_get_conn_amount_raw(const struct https_server* const);

extern uint32_t https_server_get_conn_amount(struct https_server* const);


enum websocket_op_codes {
  websocket_continuation = 0,
  websocket_text = 1,
  websocket_binary = 2,
  websocket_close = 8,
  websocket_ping = 9,
  websocket_pong = 10
};

extern int  ws(void* const, const struct http_message* const, struct http_message* const, const int);

extern int  websocket_parse(void*, uint64_t, struct http_serversock_context* const);

extern int  websocket_len(const struct http_message* const);

extern void websocket_create_message(void*, const struct http_message* const);

extern int  ws_send(void* const, void*, uint64_t, const uint8_t, const uint8_t);

#endif // yt8_UOxpTCa__YeX9iyxAAG3U0J_HwVs