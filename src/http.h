#ifndef yt8_UOxpTCa__YeX9iyxAAG3U0J_HwVs
#define yt8_UOxpTCa__YeX9iyxAAG3U0J_HwVs 1

#include "time.h"
#include "tls.h"
#include "http_p.h"
#include "compress.h"
#include "hash_table.h"

#include <stdint.h>

enum http_close_codes {
  http_unreachable,
  http_async_socket_creation_failed,
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
  uint32_t hostname_len;
  uint32_t path_len;
  uint32_t secure:1;
  uint32_t _unused:15;
  uint16_t port;
  uint16_t port_len;
};

struct http_request {
  struct http_message* request;
  struct http_parser_settings* response_settings;
  uint32_t no_cache:1;
  uint32_t compression_is_required:1;
  uint32_t _unused:30;
  /* Compression */
  uint8_t quality;
  uint8_t window_bits;
  uint8_t mem_level;
  uint8_t mode;
};

struct http_callbacks;
struct https_callbacks;

struct http_options {
  struct http_request* requests;
  struct tcp_socket_settings* tcp_settings;
  struct tls_socket_settings* tls_settings;
  struct time_manager* manager;
  struct net_epoll* epoll;
  struct addrinfo* info;
  struct http_callbacks* http_callbacks;
  struct https_callbacks* https_callbacks;
  SSL_CTX* ctx;
  
  uint64_t timeout_after;
  uint64_t read_buffer_growth;
  
  uint32_t requests_len;
  int family;
  int flags;
};

struct http_requests {
  char* msg;
  struct http_parser_settings* response_settings;
  uint32_t len;
};

struct http_context {
  struct http_requests* requests;
  struct time_manager* manager;
  char* read_buffer;
  
  struct http_parser_session session;
  struct time_timeout_ref timeout;
  uint64_t timeout_after;
  uint64_t read_buffer_growth;
  uint64_t read_used;
  uint64_t read_size;
  
  uint32_t requests_used;
  uint32_t requests_size;
  uint32_t allocated_manager:1;
  uint32_t allocated_epoll:1;
  uint32_t allocated_ctx:1;
  uint32_t expected:1;
  uint32_t _unused:28;
};


struct http_socket;

struct http_extended_async_address {
  char* hostname;
  char* service;
  struct addrinfo* hints;
  void (*callback)(struct net_async_address*, struct addrinfo*);
  struct http_socket* socket;
};

struct http_callbacks {
  void (*onresponse)(struct http_socket*, struct http_message*);
  int (*onnomem)(struct http_socket*);
  void (*onclose)(struct http_socket*, int);
};

struct http_socket {
  struct tcp_socket tcp;
  struct http_context context;
  struct http_callbacks* callbacks;
};


struct https_socket;

struct https_extended_async_address {
  char* hostname;
  char* service;
  struct addrinfo* hints;
  void (*callback)(struct net_async_address*, struct addrinfo*);
  struct https_socket* socket;
};

struct https_callbacks {
  void (*onresponse)(struct https_socket*, struct http_message*);
  int (*onnomem)(struct https_socket*);
  void (*onclose)(struct https_socket*, int);
};

struct https_socket {
  struct tls_socket tls;
  struct http_context context;
  struct https_callbacks* callbacks;
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
  char* read_buffer;
  struct http_parser_settings* settings;
  struct http_hash_table_entry* entry;
  z_stream* compressor;
  z_stream* decompressor;
  
  struct http_parser_session session;
  struct http_message message;
  struct time_timeout_ref timeout;
  struct http_parser_settings backup_settings;
  
  uint64_t read_used;
  uint64_t read_size;
  
  uint32_t expected:1;
  uint32_t protocol:1;
  uint32_t permessage_deflate:1;
  uint32_t closing:1;
  uint32_t close_onreadclose:1;
  uint32_t init_parsed:1;
  uint32_t alloc_compressor:1;
  uint32_t alloc_decompressor:1;
  uint32_t _unused:24;
};

struct http_serversock {
  struct tcp_socket tcp;
  struct http_serversock_context context;
  struct ws_serversock_callbacks* callbacks;
};

struct https_serversock {
  struct tls_socket tls;
  struct http_serversock_context context;
  struct wss_serversock_callbacks* callbacks;
};


struct http_server;
struct https_server;

struct http_resource {
  char* path;
  struct http_parser_settings* settings;
  union {
    void (*http_callback)(struct http_server*, struct http_serversock*, struct http_parser_settings*, struct http_message*, struct http_message*);
    void (*https_callback)(struct https_server*, struct https_serversock*, struct http_parser_settings*, struct http_message*, struct http_message*);
  };
};

struct http_server_callbacks;
struct https_server_callbacks;

struct http_server_options {
  struct http_resource* resources;
  struct tcp_socket_settings* tcp_socket_settings;
  struct tls_socket_settings* tls_socket_settings;
  struct tcp_server_settings* tcp_server_settings;
  struct http_parser_settings* settings;
  struct time_manager* manager;
  struct net_epoll* epoll;
  struct addrinfo* info;
  union {
    struct http_server_callbacks* http_callbacks;
    struct https_server_callbacks* https_callbacks;
  };
  union {
    struct http_server* http_server;
    struct https_server* https_server;
  };
  struct http_hash_table* table;
  SSL_CTX* ctx;
  char* cert_path;
  char* key_path;
  EVP_PKEY* key;
  
  uint64_t timeout_after;
  
  uint32_t read_buffer_growth;
  uint32_t resources_len;
  int family;
  int flags;
  int key_type;
  uint32_t socket_size;
};

struct http_server_context {
  struct time_manager* manager;
  struct tcp_socket_settings* tcp_settings;
  struct tls_socket_settings* tls_settings;
  struct http_parser_settings* settings;
  struct http_hash_table* table;
  
  uint64_t timeout_after;
  
  uint32_t read_buffer_growth;
  uint32_t allocated_table:1;
  uint32_t allocated_manager:1;
  uint32_t allocated_epoll:1;
  uint32_t allocated_ctx:1;
  uint32_t allocated_info:1;
  /* HTTP compression */
  uint32_t compression_is_required:1;
  uint32_t _unused:26;
  uint8_t quality;
  uint8_t window_bits;
  uint8_t mem_level;
  uint8_t mode;
};


struct http_server;

struct http_server_callbacks {
  int (*onnomem)(struct http_server*);
  void (*onshutdown)(struct http_server*);
};

struct http_server {
  struct tcp_server tcp;
  struct http_server_context context;
  struct http_server_callbacks* callbacks;
};


struct https_server;

struct https_server_callbacks {
  int (*onnomem)(struct https_server*);
  void (*onshutdown)(struct https_server*);
};

struct https_server {
  struct tls_server tls;
  struct http_server_context context;
  struct https_server_callbacks* callbacks;
};

extern int  http_server(char* const, struct http_server_options*);


extern void http_server_foreach_conn(struct http_server* const, void (*)(struct http_serversock*, void*), void*, const int);

extern void http_server_dont_accept_conn(struct http_server* const);

extern void http_server_accept_conn(struct http_server* const);

extern int  http_server_shutdown(struct http_server* const);

extern uint32_t http_server_get_conn_amount(struct http_server* const);


extern void https_server_foreach_conn(struct https_server* const, void (*)(struct https_serversock*, void*), void*, const int);

extern void https_server_dont_accept_conn(struct https_server* const);

extern void https_server_accept_conn(struct https_server* const);

extern int  https_server_shutdown(struct https_server* const);

extern uint32_t https_server_get_conn_amount(struct https_server* const);


#define websocket_continuation 0
#define websocket_text 1
#define websocket_binary 2
#define websocket_close 8
#define websocket_ping 9
#define websocket_pong 10

extern int  ws(void* const, const struct http_parser_settings* const, const struct http_message* const, struct http_message* const, const int);

extern int  websocket_parse(void*, uint64_t, uint64_t* const, struct http_serversock_context* const);

extern int  websocket_len(const struct http_message* const);

extern void websocket_create_message(void*, const struct http_message* const);

extern int  ws_send(void* const, void*, uint64_t, const uint8_t, const uint8_t);

#endif // yt8_UOxpTCa__YeX9iyxAAG3U0J_HwVs