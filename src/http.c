#include "http.h"
#include "debug.h"
#include "base64.h"

#include <errno.h>
#include <string.h>

static void http_client_onopen(struct tcp_socket*);

static void http_client_onmessage(struct tcp_socket*);

static int  http_client_onnomem(struct tcp_socket*);

static void http_client_onclose(struct tcp_socket*);

static void http_client_onfree(struct tcp_socket*);


static void https_client_onopen(struct tls_socket*);

static void https_client_onmessage(struct tls_socket*);

static int  https_client_onnomem(struct tls_socket*);

static void https_client_onclose(struct tls_socket*);

static void https_client_onfree(struct tls_socket*);


static void http_serversock_onopen(struct tcp_socket*);

static void http_serversock_onmessage(struct tcp_socket*);

static int  http_serversock_onnomem(struct tcp_socket*);

static void http_serversock_onclose(struct tcp_socket*);

static void http_serversock_onfree(struct tcp_socket*);


static void https_serversock_onopen(struct tls_socket*);

static void https_serversock_onmessage(struct tls_socket*);

static int  https_serversock_onnomem(struct tls_socket*);

static void https_serversock_onclose(struct tls_socket*);

static void https_serversock_onfree(struct tls_socket*);


static int  http_server_onconnection(struct tcp_socket*);

static int  http_server_onnomem(struct tcp_server*);

static void http_server_onshutdown(struct tcp_server*);


static int  https_server_onconnection(struct tls_socket*);

static int  https_server_onnomem(struct tls_server*);

static void https_server_onshutdown(struct tls_server*);


static struct tcp_socket_callbacks http_client_callbacks = {0};

static struct tcp_socket_settings  http_client_settings = { 0, 1, 0, 0, 1, 0 };

static struct tcp_socket_callbacks http_serversock_callbacks = {0};

static struct tcp_socket_settings  http_serversock_settings = { 0, 1, 0, 1, 0, 0 };


static struct tls_socket_callbacks https_client_callbacks = {0};

static struct tls_socket_settings  https_client_settings = { 0, 65536, tls_onreadclose_do_nothing };

static struct tls_socket_callbacks https_serversock_callbacks = {0};


static struct tcp_server_settings  http_server_settings = { 100, 64, 0 };

static struct tcp_server_callbacks http_server_callbacks = {0};

static struct tcp_server_settings  https_server_settings = { 100, 64, 0 };

static struct tls_server_callbacks https_server_callbacks = {0};


static struct http_parser_settings http_request_parser_settings = {
  .max_method_len = 16,
  .max_path_len = 255,
  .max_headers = 32,
  .max_header_name_len = 32,
  .max_header_value_len = 1024,
  .max_body_len = 1024000 /* ~1MB */
};

static struct http_parser_settings http_prerequest_parser_settings = {
  .max_method_len = 255,
  .max_path_len = 2048,
  .stop_at_path = 1,
  .no_character_validation = 1
};

static struct http_parser_settings http_response_parser_settings = {
  .max_reason_phrase_len = 64,
  .max_headers = 32,
  .max_header_name_len = 32,
  .max_header_value_len = 1024,
  .max_body_len = 16384000, /* ~16MB */
  .client = 1
};

static pthread_once_t __http_once = PTHREAD_ONCE_INIT;

static void __http_init(void) {
  tls_ignore_sigpipe();
  
  http_client_callbacks = (struct tcp_socket_callbacks) {
    NULL,
    http_client_onopen,
    http_client_onmessage,
    NULL,
    NULL,
    http_client_onnomem,
    http_client_onclose,
    http_client_onfree
  };
  http_serversock_callbacks = (struct tcp_socket_callbacks) {
    NULL,
    http_serversock_onopen,
    http_serversock_onmessage,
    NULL,
    NULL,
    http_serversock_onnomem,
    http_serversock_onclose,
    http_serversock_onfree
  };
  
  https_client_callbacks = (struct tls_socket_callbacks) {
    NULL,
    https_client_onopen,
    https_client_onmessage,
    NULL,
    NULL,
    https_client_onnomem,
    https_client_onclose,
    https_client_onfree
  };
  https_serversock_callbacks = (struct tls_socket_callbacks) {
    NULL,
    https_serversock_onopen,
    https_serversock_onmessage,
    NULL,
    NULL,
    https_serversock_onnomem,
    https_serversock_onclose,
    https_serversock_onfree
  };
  
  http_server_callbacks = (struct tcp_server_callbacks) {
    http_server_onconnection,
    http_server_onnomem,
    http_server_onshutdown
  };
  https_server_callbacks = (struct tls_server_callbacks) {
    https_server_onconnection,
    https_server_onnomem,
    https_server_onshutdown
  };
}

static void http_init(void) {
  (void) pthread_once(&__http_once, __http_init);
}

const char* http_str_close_reason(const int err) {
  switch(err) {
    case http_unreachable: return "http_unreachable";
    case http_async_socket_creation_failed: return "http_async_socket_creation_failed";
    case http_invalid_response: return "http_invalid_response";
    case http_no_keepalive: return "http_no_keepalive";
    case http_unexpected_close: return "http_unexpected_close";
    case http_timeouted: return "http_timeouted";
    case http_no_memory: return "http_no_memory";
    default: return "http_unknown_error";
  }
}

/* A very primitive parser, but we don't need more than that. */

static struct http_uri http_parse_uri(char* const str) {
  struct http_uri uri = {0};
  if(str[4] == 's') {
    uri.secure = 1;
  }
  uri.hostname = str + 7 + uri.secure;
  const size_t len = strlen(str);
  char* const port = memchr(uri.hostname, ':', len - 7 - uri.secure);
  if(port != NULL) {
    uri.port = atoi(port + 1);
    uri.hostname_len = (uint32_t)((uintptr_t) port - (uintptr_t) str) - 7 - uri.secure;
    uri.path = memchr(port, '/', len - ((uintptr_t) port - (uintptr_t) str));
    if(uri.path == NULL) {
      uri.path = "/";
      uri.path_len = 1;
      uri.port_len = (uint16_t)(len - 1 - ((uintptr_t) port - (uintptr_t) str));
    } else {
      uri.path_len = (uint32_t)(len - ((uintptr_t) uri.path - (uintptr_t) str));
      uri.port_len = (uint32_t)((uintptr_t) uri.path - (uintptr_t) port - 1);
    }
  } else {
    uri.path = memchr(uri.hostname, '/', len - 7 - uri.secure);
    if(uri.path == NULL) {
      uri.path = "/";
      uri.path_len = 1;
      uri.hostname_len = len - 7 - uri.secure;
    } else {
      uri.path_len = (uint32_t)(len - ((uintptr_t) uri.path - (uintptr_t) str));
      uri.hostname_len = (uint32_t)((uintptr_t) uri.path - (uintptr_t) uri.hostname);
    }
  }
  return uri;
}

static struct http_requests* http_setup_requests(struct http_uri* const uri, struct http_options* const opt) {
  struct http_request req = {0};
  if(opt->requests_len == 0) {
    opt->requests_len = 1;
    opt->requests = &req;
  }
  struct http_requests* requests = malloc(sizeof(struct http_requests) * opt->requests_len);
  if(requests == NULL) {
    return NULL;
  }
  for(uint32_t k = 0; k < opt->requests_len; ++k) {
    struct http_header headers[255];
    uint32_t idx = 0;
    uint32_t done_idx = 0;
#define set_header(a,b,c,d) \
do { \
  int done = 0; \
  if(opt->requests[k].request != NULL && opt->requests[k].request->headers != NULL) { \
    for(uint32_t i = done_idx; i < opt->requests[k].request->headers_len; ++i) { \
      if(opt->requests[k].request->headers[i].name_len == b) { \
        const int cmp = strncasecmp(opt->requests[k].request->headers[i].name, a, b); \
        if(cmp <= 0) { \
          headers[idx++] = opt->requests[k].request->headers[i]; \
          ++done_idx; \
          if(cmp == 0) { \
            /* So that we don't add 2 identical headers. Let the application \
            have priority. In other words, if it specifies User-Agent, the default \
            User-Agent header below will not be set. */ \
            done = 1; \
          } \
        } \
      } else { \
        if(!done) { \
          done = 1; \
          headers[idx++] = (struct http_header){a,c,b,0,d,0}; \
        } \
        break; \
      } \
    } \
  } \
  if(!done) { \
    headers[idx++] = (struct http_header){a,c,b,0,d,0}; \
  } \
} while(0)
    set_header("Accept", 6, "*/*", 3);
    set_header("Accept-Encoding", 15, "gzip, deflate, br", 17);
    set_header("Accept-Language", 15, "en-US,en;q=0.9", 14);
    if(opt->requests[k].no_cache) {
      set_header("Cache-Control", 13, "no-cache", 8);
    }
    if(opt->requests_len == 1) {
      set_header("Connection", 10, "close", 5);
    } else {
      set_header("Connection", 10, "keep-alive", 10);
    }
    struct http_message request;
    if(opt->requests[k].request != NULL) {
      if(opt->requests[k].request->encoding != http_e_none) {
        void* body = NULL;
        size_t body_len;
#define pick(a,b) ((a)?(a):(b))
        switch(opt->requests[k].request->encoding) {
          case http_e_gzip: {
            z_stream s = {0};
            z_stream* const stream = gzipper(&s,
              pick(opt->requests[k].quality, Z_DEFAULT_COMPRESSION),
              pick(opt->requests[k].window_bits, Z_GZIP_DEFAULT_WINDOW_BITS),
              pick(opt->requests[k].mem_level, Z_DEFAULT_MEM_LEVEL),
              pick(opt->requests[k].mode, Z_DEFAULT_STRATEGY)
            );
            if(stream == NULL) {
              break;
            }
            body = gzip(stream, opt->requests[k].request->body, opt->requests[k].request->body_len, NULL, &body_len, Z_FINISH);
            deflateEnd(stream);
            break;
          }
          case http_e_deflate: {
            z_stream s = {0};
            z_stream* const stream = deflater(&s,
              pick(opt->requests[k].quality, Z_DEFAULT_COMPRESSION),
              pick(opt->requests[k].window_bits, Z_DEFLATE_DEFAULT_WINDOW_BITS),
              pick(opt->requests[k].mem_level, Z_DEFAULT_MEM_LEVEL),
              pick(opt->requests[k].mode, Z_DEFAULT_STRATEGY)
            );
            if(stream == NULL) {
              break;
            }
            body = deflate_(stream, opt->requests[k].request->body, opt->requests[k].request->body_len, NULL, &body_len, Z_FINISH);
            deflateEnd(stream);
            break;
          }
          case http_e_brotli: {
            body = brotli_compress2(opt->requests[k].request->body, opt->requests[k].request->body_len, &body_len,
              pick(opt->requests[k].quality, BROTLI_DEFAULT_QUALITY),
              pick(opt->requests[k].window_bits, BROTLI_DEFAULT_WINDOW),
              pick(opt->requests[k].mode, BROTLI_MODE_GENERIC));
            break;
          }
          default: __builtin_unreachable();
        }
#undef pick
        if(body == NULL) {
          if(opt->requests[k].compression_is_required) {
            goto err;
          } else {
            goto out;
          }
        }
        void* const ptr = realloc(body, body_len);
        if(ptr != NULL) {
          opt->requests[k].request->body = ptr;
          opt->requests[k].request->body_len = body_len;
        } else {
          opt->requests[k].request->body = body;
        }
        opt->requests[k].request->allocated_body = 1;
        switch(opt->requests[k].request->encoding) {
          case http_e_gzip: {
            set_header("Content-Encoding", 16, "gzip", 4);
            break;
          }
          case http_e_deflate: {
            set_header("Content-Encoding", 16, "deflate", 7);
            break;
          }
          case http_e_brotli: {
            set_header("Content-Encoding", 16, "br", 2);
            break;
          }
          default: __builtin_unreachable();
        }
      }
      out:;
      request = (struct http_message) {
        .method = opt->requests[k].request->method != NULL ? opt->requests[k].request->method : "GET",
        .method_len = opt->requests[k].request->method != NULL ? opt->requests[k].request->method_len : 3,
        .path = opt->requests[k].request->path != NULL ? opt->requests[k].request->path : uri->path,
        .path_len = opt->requests[k].request->path != NULL ? opt->requests[k].request->path_len : uri->path_len,
        .headers = headers,
        .body = opt->requests[k].request->body,
        .body_len = opt->requests[k].request->body_len
      };
    } else {
      request = (struct http_message) {
        .method = "GET",
        .method_len = 3,
        .path = uri->path,
        .path_len = uri->path_len,
        .headers = headers
      };
    }
    set_header("Host", 4, uri->hostname, uri->hostname_len + (uri->port_len ? uri->port_len + 1 : 0));
    if(opt->requests[k].no_cache) {
      set_header("Pragma", 6, "no-cache", 8);
    }
    set_header("User-Agent", 10, "Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/91.0.4472.124 Safari/537.36", 91);
#undef set_header
    request.headers_len = idx;
    requests[k].len = http1_message_length(&request);
    requests[k].msg = malloc(requests[k].len);
    if(requests[k].msg == NULL) {
      if(opt->requests[k].request->allocated_body) {
        free(opt->requests[k].request->body);
      }
      goto err;
    }
    http1_create_message(requests[k].msg, &request);
    if(opt->requests[k].request != NULL && opt->requests[k].request->allocated_body) {
      free(opt->requests[k].request->body);
    }
    if(opt->requests[k].response_settings != NULL) {
      requests[k].response_settings = opt->requests[k].response_settings;
    } else {
      requests[k].response_settings = &http_response_parser_settings;
    }
    continue;
    
    err:
    while(k != 0) {
      free(requests[--k].msg);
    }
    free(requests);
    return NULL;
  }
  return requests;
}

static void http_free_requests(struct http_requests* requests, const uint32_t amount) {
  for(uint32_t i = 0; i < amount; ++i) {
    free(requests[i].msg);
  }
  free(requests);
}

static void http_free_socket(struct http_socket* const socket) {
  if(socket->context.read_buffer != NULL) {
    free(socket->context.read_buffer);
  }
  if(socket->context.allocated_epoll) {
    net_epoll_stop(socket->tcp.epoll);
    net_epoll_free(socket->tcp.epoll);
    free(socket->tcp.epoll);
  }
  if(socket->context.allocated_manager) {
    time_manager_stop(socket->context.manager);
    time_manager_free(socket->context.manager);
    free(socket->context.manager);
  }
  http_free_requests(socket->context.requests, socket->context.requests_size);
}

#define addr ((struct http_extended_async_address*) ad)

static void http_async_setup_socket(struct net_async_address* ad, struct addrinfo* info) {
  if(info == NULL) {
    addr->socket->callbacks->onclose(addr->socket, http_unreachable);
    http_free_socket(addr->socket);
    return;
  }
  addr->socket->tcp.info = info;
  while(1) {
    if(tcp_create_socket(&addr->socket->tcp) != 0) {
      if(errno == ENOMEM && addr->socket->callbacks->onnomem(addr->socket) == 0) {
        continue;
      }
      addr->socket->callbacks->onclose(addr->socket, http_async_socket_creation_failed);
      http_free_socket(addr->socket);
    }
    break;
  }
  free(ad);
}

#undef addr

#define socket ((struct http_socket*) data)

static void http_stop_socket(void* data) {
  socket->callbacks->onclose(socket, http_timeouted);
  socket->context.expected = 1;
  tcp_socket_force_close(&socket->tcp);
}

#undef socket

static int http_setup_socket(struct http_uri* const uri, struct http_options* const opt, struct http_requests* requests) {
  struct http_socket* const socket = calloc(1, sizeof(struct http_socket));
  if(socket == NULL) {
    goto err_req;
  }
  http_init();
  if(opt->tcp_settings == NULL) {
    socket->tcp.settings = &http_client_settings;
  } else {
    socket->tcp.settings = opt->tcp_settings;
  }
  socket->tcp.callbacks = &http_client_callbacks;
  socket->callbacks = opt->http_callbacks;
  if(opt->timeout_after == 0) {
    socket->context.timeout_after = 10;
  } else {
    socket->context.timeout_after = opt->timeout_after;
  }
  socket->context.requests = requests;
  socket->context.requests_size = opt->requests_len;
  if(opt->read_buffer_growth == 0) {
    socket->context.read_buffer_growth = 65536;
  } else {
    socket->context.read_buffer_growth = opt->read_buffer_growth;
  }
  if(opt->timeout_after != UINT64_MAX) {
    if(opt->manager != NULL) {
      socket->context.manager = opt->manager;
    } else {
      socket->context.manager = calloc(1, sizeof(struct time_manager));
      if(socket->context.manager == NULL) {
        goto err_sock;
      }
      if(time_manager(socket->context.manager) != 0) {
        free(socket->context.manager);
        goto err_sock;
      }
      if(time_manager_start(socket->context.manager) != 0) {
        time_manager_stop(socket->context.manager);
        free(socket->context.manager);
        goto err_sock;
      }
      socket->context.allocated_manager = 1;
    }
  }
  if(opt->epoll != NULL) {
    socket->tcp.epoll = opt->epoll;
  } else {
    socket->tcp.epoll = calloc(1, sizeof(struct net_epoll));
    if(socket->tcp.epoll == NULL) {
      goto err_time;
    }
    if(tcp_epoll(socket->tcp.epoll) != 0) {
      free(socket->tcp.epoll);
      goto err_time;
    }
    if(net_epoll_start(socket->tcp.epoll) != 0) {
      net_epoll_free(socket->tcp.epoll);
      free(socket->tcp.epoll);
      goto err_time;
    }
    socket->context.allocated_epoll = 1;
  }
  if(opt->info != NULL) {
    socket->tcp.info = opt->info;
    if(tcp_create_socket(&socket->tcp) != 0) {
      goto err_epoll;
    }
  } else {
    struct http_extended_async_address* addr = malloc(sizeof(struct http_extended_async_address) + sizeof(struct addrinfo) + uri->hostname_len + 7);
    if(addr == NULL) {
      goto err_epoll;
    }
    struct addrinfo* const hints = (struct addrinfo*)(addr + 1);
    (void) memset(hints, 0, sizeof(struct addrinfo));
    char* const hostname = (char*)(addr + 1) + sizeof(struct addrinfo);
    (void) memcpy(hostname, uri->hostname, uri->hostname_len);
    hostname[uri->hostname_len] = 0;
    addr->hostname = hostname;
    if(uri->port != 0) {
      char* const port = hostname + uri->hostname_len + 1;
      if(sprintf(port, "%hu", uri->port) < 0) {
        free(addr);
        goto err_epoll;
      }
      addr->service = port;
    } else {
      addr->service = "80";
    }
    *hints = net_get_addr_struct(opt->family, stream_socktype, tcp_protocol, opt->flags);
    addr->socket = socket;
    addr->callback = http_async_setup_socket;
    addr->hints = hints;
    if(net_get_address_async((struct net_async_address*) addr) != 0) {
      free(addr);
      goto err_epoll;
    }
  }
  
  return 0;
  
  err_epoll:
  if(socket->context.allocated_epoll) {
    net_epoll_stop(socket->tcp.epoll);
    net_epoll_free(socket->tcp.epoll);
    free(socket->tcp.epoll);
  }
  err_time:
  if(socket->context.allocated_manager) {
    time_manager_stop(socket->context.manager);
    time_manager_free(socket->context.manager);
    free(socket->context.manager);
  }
  err_sock:
  free(socket);
  err_req:
  http_free_requests(requests, opt->requests_len);
  return -1;
}

static void https_free_socket(struct https_socket* const socket) {
  if(socket->context.allocated_ctx) {
    SSL_CTX_free(socket->tls.ctx);
  }
  if(socket->context.allocated_epoll) {
    net_epoll_stop(socket->tls.tcp.epoll);
    net_epoll_free(socket->tls.tcp.epoll);
    free(socket->tls.tcp.epoll);
  }
  if(socket->context.allocated_manager) {
    time_manager_stop(socket->context.manager);
    time_manager_free(socket->context.manager);
    free(socket->context.manager);
  }
  http_free_requests(socket->context.requests, socket->context.requests_size);
}

#define addr ((struct https_extended_async_address*) ad)

static void https_async_setup_socket(struct net_async_address* ad, struct addrinfo* info) {
  if(info == NULL) {
    addr->socket->callbacks->onclose(addr->socket, http_unreachable);
    https_free_socket(addr->socket);
    return;
  }
  addr->socket->tls.tcp.info = info;
  while(1) {
    if(tls_create_socket(&addr->socket->tls) != 0) {
      if(errno == ENOMEM && addr->socket->callbacks->onnomem(addr->socket) == 0) {
        continue;
      }
      addr->socket->callbacks->onclose(addr->socket, http_async_socket_creation_failed);
      https_free_socket(addr->socket);
    }
    break;
  }
  free(ad);
}

#undef addr

#define socket ((struct https_socket*) data)

static void https_stop_socket(void* data) {
  socket->callbacks->onclose(socket, http_timeouted);
  socket->context.expected = 1;
  tcp_socket_force_close(&socket->tls.tcp);
}

#undef socket

static int https_setup_socket(struct http_uri* const uri, struct http_options* const opt, struct http_requests* requests) {
  struct https_socket* const socket = calloc(1, sizeof(struct https_socket));
  if(socket == NULL) {
    goto err_req;
  }
  http_init();
  if(opt->tcp_settings == NULL) {
    socket->tls.tcp.settings = &http_client_settings;
  } else {
    socket->tls.tcp.settings = opt->tcp_settings;
  }
  if(opt->tls_settings == NULL) {
    socket->tls.settings = &https_client_settings;
  } else {
    socket->tls.settings = opt->tls_settings;
  }
  socket->tls.callbacks = &https_client_callbacks;
  socket->callbacks = opt->https_callbacks;
  if(opt->timeout_after == 0) {
    socket->context.timeout_after = 10;
  } else {
    socket->context.timeout_after = opt->timeout_after;
  }
  socket->context.requests = requests;
  socket->context.requests_size = opt->requests_len;
  if(opt->timeout_after != UINT64_MAX) {
    if(opt->manager != NULL) {
      socket->context.manager = opt->manager;
    } else {
      socket->context.manager = calloc(1, sizeof(struct time_manager));
      if(socket->context.manager == NULL) {
        goto err_sock;
      }
      if(time_manager(socket->context.manager) != 0) {
        free(socket->context.manager);
        goto err_sock;
      }
      if(time_manager_start(socket->context.manager) != 0) {
        time_manager_stop(socket->context.manager);
        free(socket->context.manager);
        goto err_sock;
      }
      socket->context.allocated_manager = 1;
    }
  }
  if(opt->epoll != NULL) {
    socket->tls.tcp.epoll = opt->epoll;
  } else {
    socket->tls.tcp.epoll = calloc(1, sizeof(struct net_epoll));
    if(socket->tls.tcp.epoll == NULL) {
      goto err_time;
    }
    if(tls_epoll(socket->tls.tcp.epoll) != 0) {
      free(socket->tls.tcp.epoll);
      goto err_time;
    }
    if(net_epoll_start(socket->tls.tcp.epoll) != 0) {
      net_epoll_free(socket->tls.tcp.epoll);
      free(socket->tls.tcp.epoll);
      goto err_time;
    }
    socket->context.allocated_epoll = 1;
  }
  if(opt->ctx != NULL) {
    socket->tls.ctx = opt->ctx;
  } else {
    socket->tls.ctx = SSL_CTX_new(TLS_client_method());
    if(socket->tls.ctx == NULL) {
      goto err_epoll;
    }
    socket->context.allocated_ctx = 1;
    if(SSL_CTX_set_min_proto_version(socket->tls.ctx, TLS1_2_VERSION) == 0) {
      goto err_ctx;
    }
  }
  if(opt->info != NULL) {
    uint32_t allocated_canonname = 0;
    socket->tls.tcp.info = opt->info;
    if(opt->info->ai_canonname == NULL) {
      opt->info->ai_canonname = malloc(uri->hostname_len + 1);
      if(opt->info->ai_canonname == NULL) {
        goto err_ctx;
      }
      allocated_canonname = 1;
      (void) memcpy(opt->info->ai_canonname, uri->hostname, uri->hostname_len);
      opt->info->ai_canonname[uri->hostname_len] = 0;
    }
    if(tls_create_socket(&socket->tls) != 0) {
      if(allocated_canonname) {
        free(opt->info->ai_canonname);
      }
      goto err_ctx;
    }
  } else {
    struct https_extended_async_address* addr = malloc(sizeof(struct https_extended_async_address) + sizeof(struct addrinfo) + uri->hostname_len + 7);
    if(addr == NULL) {
      goto err_ctx;
    }
    struct addrinfo* const hints = (struct addrinfo*)(addr + 1);
    (void) memset(hints, 0, sizeof(struct addrinfo));
    char* const hostname = (char*)(addr + 1) + sizeof(struct addrinfo);
    (void) memcpy(hostname, uri->hostname, uri->hostname_len);
    hostname[uri->hostname_len] = 0;
    addr->hostname = hostname;
    if(uri->port != 0) {
      char* const port = hostname + uri->hostname_len + 1;
      if(sprintf(port, "%hu", uri->port) < 0) {
        free(addr);
        goto err_ctx;
      }
      addr->service = port;
    } else {
      addr->service = "443";
    }
    *hints = net_get_addr_struct(opt->family, stream_socktype, tcp_protocol, opt->flags | AI_CANONNAME);
    addr->socket = socket;
    addr->callback = https_async_setup_socket;
    addr->hints = hints;
    if(net_get_address_async((struct net_async_address*) addr) != 0) {
      free(addr);
      goto err_ctx;
    }
  }
  return 0;
  
  err_ctx:
  if(socket->context.allocated_ctx) {
    SSL_CTX_free(socket->tls.ctx);
  }
  err_epoll:
  if(socket->context.allocated_epoll) {
    net_epoll_stop(socket->tls.tcp.epoll);
    net_epoll_free(socket->tls.tcp.epoll);
    free(socket->tls.tcp.epoll);
  }
  err_time:
  if(socket->context.allocated_manager) {
    time_manager_stop(socket->context.manager);
    time_manager_free(socket->context.manager);
    free(socket->context.manager);
  }
  err_sock:
  free(socket);
  err_req:
  http_free_requests(requests, opt->requests_len);
  return -1;
}



#define socket ((struct http_socket*) soc)

static void http_client_onopen(struct tcp_socket* soc) {
  (void) tcp_socket_keepalive(&socket->tcp);
  if(socket->context.timeout_after != UINT64_MAX) {
    while(1) {
      const int err = time_manager_add_timeout(socket->context.manager, time_get_sec(socket->context.timeout_after), http_stop_socket, socket, &socket->context.timeout);
      if(err != 0) {
        if(socket->callbacks->onnomem(socket) == 0) {
          continue;
        }
        socket->callbacks->onclose(socket, http_no_memory);
        socket->context.expected = 1;
        tcp_socket_force_close(&socket->tcp);
        return;
      }
      break;
    }
  }
  (void) tcp_send(&socket->tcp, socket->context.requests[socket->context.requests_used].msg, socket->context.requests[socket->context.requests_used].len);
}

static void http_client_onmessage(struct tcp_socket* soc) {
  while(1) {
    if(socket->context.read_used + socket->context.read_buffer_growth > socket->context.read_size) {
      while(1) {
        char* const ptr = realloc(socket->context.read_buffer, socket->context.read_used + socket->context.read_buffer_growth);
        if(ptr != NULL) {
          socket->context.read_buffer = ptr;
          socket->context.read_size = socket->context.read_used + socket->context.read_buffer_growth;
          break;
        }
        if(socket->callbacks->onnomem(socket) == 0) {
          continue;
        }
        (void) time_manager_cancel_timeout(socket->context.manager, &socket->context.timeout);
        socket->callbacks->onclose(socket, http_no_memory);
        socket->context.expected = 1;
        tcp_socket_force_close(&socket->tcp);
        return;
      }
    }
    const uint64_t to_read = socket->context.read_size - socket->context.read_used;
    const uint64_t read = tcp_read(&socket->tcp, socket->context.read_buffer + socket->context.read_used, to_read);
    socket->context.read_used += read;
    if(read < to_read) {
      break;
    }
  }
  struct http_header headers[255];
  struct http_message response = {0};
  response.headers = headers;
  response.headers_len = 255;
  int state;
  while(1) {
    state = http1_parse_response(socket->context.read_buffer, socket->context.read_used, &socket->context.read_used, &socket->context.session, socket->context.requests[socket->context.requests_used].response_settings, &response);
    if(state == http_out_of_memory) {
      if(socket->callbacks->onnomem(socket) == 0) {
        continue;
      }
      (void) time_manager_cancel_timeout(socket->context.manager, &socket->context.timeout);
      socket->callbacks->onclose(socket, http_no_memory);
      socket->context.expected = 1;
      goto err_buf;
    }
    break;
  }
  if(state != http_incomplete) {
    (void) time_manager_cancel_timeout(socket->context.manager, &socket->context.timeout);
    socket->context.session = (struct http_parser_session){0};
    if(state == http_valid) {
      socket->callbacks->onresponse(socket, &response);
    } else {
      errno = EINVAL;
      socket->callbacks->onclose(socket, http_invalid_response);
      socket->context.expected = 1;
      goto err_res;
    }
    if(++socket->context.requests_used == socket->context.requests_size) {
      goto err_res;
    } else {
      const struct http_header* const connection = http1_seek_header(&response, "connection", 10);
      const int conn_is_close = connection != NULL && connection->value_len == 5 && strncasecmp(connection->value, "close", 5) == 0;
      if(response.allocated_body) {
        free(response.body);
      }
      free(socket->context.read_buffer);
      socket->context.read_buffer = NULL;
      if(conn_is_close) {
        socket->callbacks->onclose(socket, http_no_keepalive);
        socket->context.expected = 1;
        goto err;
      } else {
        socket->context.read_used = 0;
        socket->context.read_size = 0;
        http_client_onopen(&socket->tcp);
      }
    }
    return;
    
    err_res:
    if(response.allocated_body) {
      free(response.body);
    }
    goto err_buf;
  }
  return;
  
  err_buf:
  free(socket->context.read_buffer);
  socket->context.read_buffer = NULL;
  err:
  tcp_socket_force_close(&socket->tcp);
}

static int http_client_onnomem(struct tcp_socket* soc) {
  return socket->callbacks->onnomem(socket);
}

static void http_client_onclose(struct tcp_socket* soc) {
  if(!socket->context.expected) {
    (void) time_manager_cancel_timeout(socket->context.manager, &socket->context.timeout);
    if(socket->context.requests_used != socket->context.requests_size) {
      socket->callbacks->onclose(socket, http_unexpected_close);
    }
  }
  tcp_socket_free(&socket->tcp);
}

static void http_client_onfree(struct tcp_socket* soc) {
  http_free_socket(socket);
}

#undef socket



#define socket ((struct https_socket*) soc)

static void https_client_onopen(struct tls_socket* soc) {
  (void) tcp_socket_keepalive(&socket->tls.tcp);
  if(socket->context.timeout_after != UINT64_MAX) {
    while(1) {
      const int err = time_manager_add_timeout(socket->context.manager, time_get_sec(socket->context.timeout_after), https_stop_socket, socket, &socket->context.timeout);
      if(err != 0) {
        if(socket->callbacks->onnomem(socket) == 0) {
          continue;
        }
        socket->callbacks->onclose(socket, http_no_memory);
        socket->context.expected = 1;
        tcp_socket_force_close(&socket->tls.tcp);
        return;
      }
      break;
    }
  }
  (void) tls_send(&socket->tls, socket->context.requests[socket->context.requests_used].msg, socket->context.requests[socket->context.requests_used].len);
}

static void https_client_onmessage(struct tls_socket* soc) {
  /* Optimised version of http_client_onmessage here. We don't need to allocate
  a single piece of memory. The fact that we are in epoll right now also adds up
  to gained speed, since absolutely no race conditions and lock contention. */
  struct http_header headers[255];
  struct http_message response = {0};
  response.headers = headers;
  response.headers_len = 255;
  int state;
  while(1) {
    state = http1_parse_response(socket->tls.read_buffer, socket->tls.read_used, &socket->tls.read_used, &socket->context.session, socket->context.requests[socket->context.requests_used].response_settings, &response);
    if(state == http_out_of_memory) {
      if(socket->callbacks->onnomem(socket) == 0) {
        continue;
      }
      (void) time_manager_cancel_timeout(socket->context.manager, &socket->context.timeout);
      socket->callbacks->onclose(socket, http_no_memory);
      socket->context.expected = 1;
      goto err_buf;
    }
    break;
  }
  if(state != http_incomplete) {
    (void) time_manager_cancel_timeout(socket->context.manager, &socket->context.timeout);
    socket->context.session = (struct http_parser_session){0};
    if(state == http_valid) {
      socket->callbacks->onresponse(socket, &response);
    } else {
      errno = EINVAL;
      socket->callbacks->onclose(socket, http_invalid_response);
      socket->context.expected = 1;
      goto err_res;
    }
    if(++socket->context.requests_used == socket->context.requests_size) {
      goto err_res;
    } else {
      const struct http_header* const connection = http1_seek_header(&response, "connection", 10);
      const int conn_is_close = connection != NULL && connection->value_len == 5 && strncasecmp(connection->value, "close", 5) == 0;
      if(response.allocated_body) {
        free(response.body);
      }
      free(socket->tls.read_buffer);
      socket->tls.read_buffer = NULL;
      socket->tls.read_used = 0;
      socket->tls.read_size = 0;
      if(conn_is_close) {
        socket->callbacks->onclose(socket, http_no_keepalive);
        socket->context.expected = 1;
        goto err;
      } else {
        https_client_onopen(&socket->tls);
      }
    }
    return;
    
    err_res:
    if(response.allocated_body) {
      free(response.body);
    }
    goto err_buf;
  }
  return;
  
  err_buf:
  free(socket->tls.read_buffer);
  socket->tls.read_buffer = NULL;
  socket->tls.read_used = 0;
  socket->tls.read_size = 0;
  err:
  tcp_socket_force_close(&socket->tls.tcp);
}

static int https_client_onnomem(struct tls_socket* soc) {
  return socket->callbacks->onnomem(socket);
}

static void https_client_onclose(struct tls_socket* soc) {
  if(!socket->context.expected) {
    (void) time_manager_cancel_timeout(socket->context.manager, &socket->context.timeout);
    if(socket->context.requests_used != socket->context.requests_size) {
      socket->callbacks->onclose(socket, http_unexpected_close);
    }
  }
  tls_socket_free(&socket->tls);
}

static void https_client_onfree(struct tls_socket* soc) {
  https_free_socket(socket);
}

#undef socket

int http(char* const str, struct http_options* options) {
  struct http_uri uri = http_parse_uri(str);
  struct http_options opt = {0};
  if(options == NULL) {
    options = &opt;
  }
  struct http_requests* requests = http_setup_requests(&uri, options);
  if(requests == NULL) {
    return -1;
  }
  if(uri.secure) {
    if(https_setup_socket(&uri, options, requests) != 0) {
      return -1;
    }
  } else {
    if(http_setup_socket(&uri, options, requests) != 0) {
      return -1;
    }
  }
  return 0;
}



static int http_setup_server(struct http_uri* const uri, struct http_server_options* const opt) {
  struct http_server* const server = calloc(1, sizeof(struct http_server));
  if(server == NULL) {
    return -1;
  }
  http_init();
  if(opt->tcp_socket_settings == NULL) {
    server->context.tcp_settings = &http_serversock_settings;
  } else {
    server->context.tcp_settings = opt->tcp_socket_settings;
  }
  server->callbacks = opt->http_callbacks;
  if(opt->timeout_after == 0) {
    server->context.timeout_after = 10;
  } else {
    server->context.timeout_after = opt->timeout_after;
  }
  if(opt->tcp_server_settings == NULL) {
    server->tcp.settings = &http_server_settings;
  } else {
    server->tcp.settings = opt->tcp_server_settings;
  }
  server->tcp.callbacks = &http_server_callbacks;
  if(opt->socket_size == 0) {
    server->tcp.socket_size = sizeof(struct http_serversock);
  } else {
    server->tcp.socket_size = opt->socket_size;
  }
  if(opt->settings == NULL) {
    server->context.settings = &http_request_parser_settings;
  } else {
    server->context.settings = opt->settings;
  }
  if(opt->read_buffer_growth == 0) {
    server->context.read_buffer_growth = 65536;
  } else {
    server->context.read_buffer_growth = opt->read_buffer_growth;
  }
  if(opt->table != NULL) {
    server->context.table = opt->table;
  } else {
    server->context.table = calloc(1, sizeof(struct http_hash_table));
    if(server->context.table == NULL) {
      goto err_serv;
    }
    if(http_hash_table_init(server->context.table, opt->resources_len << 1) != 0) {
      goto err_serv;
    }
    for(uint32_t i = 0; i < opt->resources_len; ++i) {
      http_hash_table_insert(server->context.table, opt->resources[i].path, opt->resources[i].settings, (http_hash_table_func_t) opt->resources[i].http_callback);
    }
    server->context.allocated_table = 1;
  }
  if(opt->timeout_after != UINT64_MAX) {
    if(opt->manager != NULL) {
      server->context.manager = opt->manager;
    } else {
      server->context.manager = calloc(1, sizeof(struct time_manager));
      if(server->context.manager == NULL) {
        goto err_table;
      }
      if(time_manager(server->context.manager) != 0) {
        free(server->context.manager);
        goto err_table;
      }
      if(time_manager_start(server->context.manager) != 0) {
        time_manager_stop(server->context.manager);
        free(server->context.manager);
        goto err_table;
      }
      server->context.allocated_manager = 1;
    }
  }
  if(opt->epoll != NULL) {
    server->tcp.epoll = opt->epoll;
  } else {
    server->tcp.epoll = calloc(1, sizeof(struct net_epoll));
    if(server->tcp.epoll == NULL) {
      goto err_time;
    }
    if(tcp_epoll(server->tcp.epoll) != 0) {
      free(server->tcp.epoll);
      goto err_time;
    }
    if(net_epoll_start(server->tcp.epoll) != 0) {
      net_epoll_free(server->tcp.epoll);
      free(server->tcp.epoll);
      goto err_time;
    }
    server->context.allocated_epoll = 1;
  }
  if(opt->info == NULL) {
    struct addrinfo hints = net_get_addr_struct(opt->family, stream_socktype, tcp_protocol, opt->flags | wants_a_server);
    char port[6];
    if(uri->port != 0) {
      if(sprintf(port, "%hu", uri->port) < 0) {
        goto err_epoll;
      }
    } else {
      port[0] = '8';
      port[1] = '0';
      port[2] = 0;
    }
    char* hostname = malloc(uri->hostname_len + 1);
    if(hostname == NULL) {
      goto err_epoll;
    }
    (void) memcpy(hostname, uri->hostname, uri->hostname_len);
    hostname[uri->hostname_len] = 0;
    struct addrinfo* const info = net_get_address(hostname, port, &hints);
    free(hostname);
    if(info == NULL) {
      goto err_epoll;
    }
    opt->info = info;
    server->context.allocated_info = 1;
  }
  if(tcp_create_server(&server->tcp, opt->info) != 0) {
    goto err_info;
  }
  if(server->context.allocated_info) {
    net_get_address_free(opt->info);
  }
  opt->http_server = server;
  return 0;
  
  err_info:
  if(server->context.allocated_info) {
    net_get_address_free(opt->info);
  }
  err_epoll:
  if(server->context.allocated_epoll) {
    net_epoll_stop(server->tcp.epoll);
    net_epoll_free(server->tcp.epoll);
    free(server->tcp.epoll);
  }
  err_time:
  if(server->context.allocated_manager) {
    time_manager_stop(server->context.manager);
    time_manager_free(server->context.manager);
    free(server->context.manager);
  }
  err_table:
  if(server->context.allocated_table) {
    http_hash_table_free(server->context.table);
    free(server->context.table);
  }
  err_serv:
  free(server);
  return -1;
}

void http_server_foreach_conn(struct http_server* const server, void (*callback)(struct http_serversock*, void*), void* data, const int write) {
  tcp_server_foreach_conn(&server->tcp, (void (*)(struct tcp_socket*,void*)) callback, data, write);
}

void http_server_dont_accept_conn(struct http_server* const server) {
  tcp_server_dont_accept_conn(&server->tcp);
}

void http_server_accept_conn(struct http_server* const server) {
  tcp_server_accept_conn(&server->tcp);
}

int http_server_shutdown(struct http_server* const server) {
  return tcp_server_shutdown(&server->tcp);
}

uint32_t http_server_get_conn_amount_raw(const struct http_server* const server) {
  return tcp_server_get_conn_amount_raw(&server->tcp);
}

uint32_t http_server_get_conn_amount(struct http_server* const server) {
  return tcp_server_get_conn_amount(&server->tcp);
}



static int https_setup_server(struct http_uri* const uri, struct http_server_options* const opt) {
  struct https_server* const server = calloc(1, sizeof(struct https_server));
  if(server == NULL) {
    return -1;
  }
  http_init();
  if(opt->tcp_socket_settings == NULL) {
    server->context.tcp_settings = &http_serversock_settings;
  } else {
    server->context.tcp_settings = opt->tcp_socket_settings;
  }
  if(opt->tls_socket_settings != NULL) {
    server->context.tls_settings = opt->tls_socket_settings;
  } /* else default */
  server->callbacks = opt->https_callbacks;
  if(opt->timeout_after == 0) {
    server->context.timeout_after = 10;
  } else {
    server->context.timeout_after = opt->timeout_after;
  }
  if(opt->tcp_server_settings == NULL) {
    server->tls.tcp.settings = &https_server_settings;
  } else {
    server->tls.tcp.settings = opt->tcp_server_settings;
  }
  server->tls.callbacks = &https_server_callbacks;
  if(opt->socket_size == 0) {
    server->tls.tcp.socket_size = sizeof(struct https_serversock);
  } else {
    server->tls.tcp.socket_size = opt->socket_size;
  }
  if(opt->settings == NULL) {
    server->context.settings = &http_request_parser_settings;
  } else {
    server->context.settings = opt->settings;
  }
  if(opt->read_buffer_growth == 0) {
    server->context.read_buffer_growth = 65536;
  } else {
    server->context.read_buffer_growth = opt->read_buffer_growth;
  }
  if(opt->table != NULL) {
    server->context.table = opt->table;
  } else {
    server->context.table = calloc(1, sizeof(struct http_hash_table));
    if(server->context.table == NULL) {
      goto err_serv;
    }
    if(http_hash_table_init(server->context.table, opt->resources_len << 1) != 0) {
      free(server->context.table);
      goto err_serv;
    }
    for(uint32_t i = 0; i < opt->resources_len; ++i) {
      http_hash_table_insert(server->context.table, opt->resources[i].path, opt->resources[i].settings, (http_hash_table_func_t) opt->resources[i].https_callback);
    }
    server->context.allocated_table = 1;
  }
  if(opt->timeout_after != UINT64_MAX) {
    if(opt->manager != NULL) {
      server->context.manager = opt->manager;
    } else {
      server->context.manager = calloc(1, sizeof(struct time_manager));
      if(server->context.manager == NULL) {
        goto err_table;
      }
      if(time_manager(server->context.manager) != 0) {
        free(server->context.manager);
        goto err_table;
      }
      if(time_manager_start(server->context.manager) != 0) {
        time_manager_stop(server->context.manager);
        free(server->context.manager);
        goto err_table;
      }
      server->context.allocated_manager = 1;
    }
  }
  if(opt->epoll != NULL) {
    server->tls.tcp.epoll = opt->epoll;
  } else {
    server->tls.tcp.epoll = calloc(1, sizeof(struct net_epoll));
    if(server->tls.tcp.epoll == NULL) {
      goto err_time;
    }
    if(tcp_epoll(server->tls.tcp.epoll) != 0) {
      free(server->tls.tcp.epoll);
      goto err_time;
    }
    if(net_epoll_start(server->tls.tcp.epoll) != 0) {
      net_epoll_free(server->tls.tcp.epoll);
      free(server->tls.tcp.epoll);
      goto err_time;
    }
    server->context.allocated_epoll = 1;
  }
  if(opt->ctx != NULL) {
    server->tls.ctx = opt->ctx;
  } else {
    server->tls.ctx = SSL_CTX_new(TLS_server_method());
    if(server->tls.ctx == NULL) {
      goto err_epoll;
    }
    server->context.allocated_ctx = 1;
    if(SSL_CTX_use_certificate_chain_file(server->tls.ctx, opt->cert_path) != 1) {
      goto err_ctx;
    }
    if(opt->key_path != NULL) {
      if(opt->key_rsa) {
        if(SSL_CTX_use_RSAPrivateKey_file(server->tls.ctx, opt->key_path, SSL_FILETYPE_PEM) != 1) {
          goto err_ctx;
        }
      } else {
        if(SSL_CTX_use_PrivateKey_file(server->tls.ctx, opt->key_path, SSL_FILETYPE_PEM) != 1) {
          goto err_ctx;
        }
      }
    } else if(opt->key != NULL) {
      if(opt->key_rsa) {
        if(SSL_CTX_use_RSAPrivateKey(server->tls.ctx, opt->rsa_key) != 1) {
          goto err_ctx;
        }
      } else {
        if(SSL_CTX_use_PrivateKey(server->tls.ctx, opt->key) != 1) {
          goto err_ctx;
        }
      }
    }
    if(SSL_CTX_set_min_proto_version(server->tls.ctx, TLS1_2_VERSION) == 0) {
      goto err_ctx;
    }
  }
  if(opt->info == NULL) {
    struct addrinfo const hints = net_get_addr_struct(opt->family, stream_socktype, tcp_protocol, opt->flags | wants_a_server);
    char port[6];
    if(uri->port != 0) {
      if(sprintf(port, "%hu", uri->port) < 0) {
        goto err_ctx;
      }
    } else {
      port[0] = '4';
      port[1] = '4';
      port[2] = '3';
      port[3] = 0;
    }
    char* hostname = malloc(uri->hostname_len + 1);
    if(hostname == NULL) {
      goto err_epoll;
    }
    (void) memcpy(hostname, uri->hostname, uri->hostname_len);
    hostname[uri->hostname_len] = 0;
    struct addrinfo* const info = net_get_address(hostname, port, &hints);
    free(hostname);
    if(info == NULL) {
      goto err_ctx;
    }
    opt->info = info;
    server->context.allocated_info = 1;
  }
  if(tls_create_server(&server->tls, opt->info) != 0) {
    goto err_info;
  }
  if(server->context.allocated_info) {
    net_get_address_free(opt->info);
  }
  opt->https_server = server;
  return 0;
  
  err_info:
  if(server->context.allocated_info) {
    net_get_address_free(opt->info);
  }
  err_ctx:
  if(server->context.allocated_ctx) {
    SSL_CTX_free(server->tls.ctx);
  }
  err_epoll:
  if(server->context.allocated_epoll) {
    net_epoll_stop(server->tls.tcp.epoll);
    net_epoll_free(server->tls.tcp.epoll);
    free(server->tls.tcp.epoll);
  }
  err_time:
  if(server->context.allocated_manager) {
    time_manager_stop(server->context.manager);
    time_manager_free(server->context.manager);
    free(server->context.manager);
  }
  err_table:
  if(server->context.allocated_table) {
    http_hash_table_free(server->context.table);
    free(server->context.table);
  }
  err_serv:
  free(server);
  return -1;
}

void https_server_foreach_conn(struct https_server* const server, void (*callback)(struct https_serversock*, void*), void* data, const int write) {
  tls_server_foreach_conn(&server->tls, (void (*)(struct tls_socket*,void*)) callback, data, write);
}

void https_server_dont_accept_conn(struct https_server* const server) {
  tls_server_dont_accept_conn(&server->tls);
}

void https_server_accept_conn(struct https_server* const server) {
  tls_server_accept_conn(&server->tls);
}

int https_server_shutdown(struct https_server* const server) {
  return tls_server_shutdown(&server->tls);
}

uint32_t https_server_get_conn_amount_raw(const struct https_server* const server) {
  return tls_server_get_conn_amount_raw(&server->tls);
}

uint32_t https_server_get_conn_amount(struct https_server* const server) {
  return tls_server_get_conn_amount(&server->tls);
}



#define http_server ((struct http_server*) socket->tcp.server)
#define https_server ((struct https_server*) socket->tcp.server)

static int http_serversock_timeout(struct http_serversock* const socket, char** const ret_msg, uint32_t* const ret_len) {
  struct http_header headers[3];
  struct http_message response = {0};
  response.headers = headers;
  response.status_code = http_s_request_timeout;
  response.reason_phrase = http1_default_reason_phrase(response.status_code);
  response.reason_phrase_len = http1_default_reason_phrase_len(response.status_code);
  
  char date_header[30];
  date_header[0] = 0;
  struct tm tms;
  if(gmtime_r(&(time_t){time(NULL)}, &tms) != NULL && strftime(date_header, 30, "%a, %d %b %Y %H:%M:%S GMT", &tms) != 0) {
    headers[0] = (struct http_header) { "Connection", "close", 10, 0, 5, 0 };
    headers[1] = (struct http_header) { "Date", date_header, 4, 0, 29, 0 };
    headers[2] = (struct http_header) { "Server", "shnet", 6, 0, 5, 0 };
    response.headers_len = 3;
  } else {
    headers[0] = (struct http_header) { "Connection", "close", 10, 0, 5, 0 };
    headers[1] = (struct http_header) { "Server", "shnet", 6, 0, 5, 0 };
    response.headers_len = 2;
  }
  char* msg;
  const uint32_t len = http1_message_length(&response);
  while(1) {
    msg = malloc(len);
    if(msg == NULL) {
      if(((struct net_socket_base*) http_server)->which & net_secure) {
        if(https_server->callbacks->onnomem(https_server) == 0) {
          continue;
        }
      } else {
        if(http_server->callbacks->onnomem(http_server) == 0) {
          continue;
        }
      }
      return -1;
    }
    break;
  }
  http1_create_message(msg, &response);
  *ret_msg = msg;
  *ret_len = len;
  return 0;
}

#undef https_server
#undef http_server

#define socket ((struct http_serversock*) data)
#define server ((struct http_server*) socket->tcp.server)

static void http_serversock_stop_socket(void* data) {
  socket->context.expected = 1;
  tcp_socket_stop_receiving_data(&socket->tcp);
  char* msg;
  uint32_t len;
  if(http_serversock_timeout(socket, &msg, &len) == 0) {
    (void) tcp_send(&socket->tcp, msg, len);
    free(msg);
  }
  tcp_socket_linger(&socket->tcp, 5);
  tcp_socket_close(&socket->tcp);
}

#undef server
#undef socket

static void http_server_free(struct http_server* const server) {
  if(server->context.allocated_epoll) {
    net_epoll_stop(server->tcp.epoll);
    net_epoll_free(server->tcp.epoll);
    free(server->tcp.epoll);
  }
  if(server->context.allocated_manager) {
    time_manager_stop(server->context.manager);
    time_manager_free(server->context.manager);
    free(server->context.manager);
  }
  if(server->context.allocated_table) {
    http_hash_table_free(server->context.table);
    free(server->context.table);
  }
}

#define server ((struct http_server*) serv)

static int http_server_onnomem(struct tcp_server* serv) {
  return server->callbacks->onnomem(server);
}

static void http_server_onshutdown(struct tcp_server* serv) {
  server->callbacks->onshutdown(server);
  http_server_free(server);
  tcp_server_free(&server->tcp);
  free(server);
}

#undef server

#define server ((struct http_server*) socket->tcp.server)

static void http_serversock_send(struct http_serversock* const socket, struct http_message* const message) {
  char* msg;
  const uint32_t len = http1_message_length(message);
  while(1) {
    msg = malloc(len);
    if(msg == NULL) {
      if(server->callbacks->onnomem(server) == 0) {
        continue;
      }
      (void) time_manager_cancel_timeout(server->context.manager, &socket->context.timeout);
      if(message->allocated_body) {
        free(message->body);
      }
      free(socket->context.read_buffer);
      socket->context.read_buffer = NULL;
      tcp_socket_force_close(&socket->tcp);
      return;
    }
    break;
  }
  http1_create_message(msg, message);
  (void) tcp_send(&socket->tcp, msg, len);
  free(msg);
}

#undef server

#define socket ((struct http_serversock*) soc)
#define server ((struct http_server*) soc->server)

static int http_server_onconnection(struct tcp_socket* soc) {
  socket->tcp.settings = server->context.tcp_settings;
  socket->tcp.callbacks = &http_serversock_callbacks;
  return 0;
}

/* Serversocks are significantly smaller than normal sockets, because we can
offload some of the needed structures on the server. Since these structures
never change, that's completely fine and we improve memory usage. */

static void http_serversock_onopen(struct tcp_socket* soc) {
  if(server->context.timeout_after != UINT64_MAX) {
    while(1) {
      const int err = time_manager_add_timeout(server->context.manager, time_get_sec(server->context.timeout_after), http_serversock_stop_socket, socket, &socket->context.timeout);
      if(err != 0) {
        if(server->callbacks->onnomem(server) == 0) {
          continue;
        }
        tcp_socket_force_close(&socket->tcp);
        return;
      }
      break;
    }
  }
}

static void http_serversock_onmessage(struct tcp_socket* soc) {
  while(1) {
    if(socket->context.read_used + server->context.read_buffer_growth > socket->context.read_size) {
      while(1) {
        char* const ptr = realloc(socket->context.read_buffer, socket->context.read_used + server->context.read_buffer_growth);
        if(ptr != NULL) {
          socket->context.read_buffer = ptr;
          socket->context.read_size = socket->context.read_used + server->context.read_buffer_growth;
          break;
        }
        if(server->callbacks->onnomem(server) == 0) {
          continue;
        }
        (void) time_manager_cancel_timeout(server->context.manager, &socket->context.timeout);
        if(socket->context.read_buffer != NULL) {
          free(socket->context.read_buffer);
          socket->context.read_buffer = NULL;
        }
        tcp_socket_force_close(&socket->tcp);
        return;
      }
    }
    const int to_read = socket->context.read_size - socket->context.read_used;
    const int read = tcp_read(&socket->tcp, socket->context.read_buffer + socket->context.read_used, to_read);
    socket->context.read_used += read;
    if(read < to_read) {
      break;
    }
  }
  if(socket->context.read_used == 0) {
    free(socket->context.read_buffer);
    socket->context.read_buffer = NULL;
    socket->context.read_size = 0;
    return;
  }
  if(socket->context.protocol == http_p_websocket) {
    goto websocket;
  }
  struct http_header req_headers[255];
  struct http_message request = {0};
  request.headers = req_headers;
  request.headers_len = 255;
  int state;
  if(socket->context.init_parsed == 0) {
    state = http1_parse_request(socket->context.read_buffer, socket->context.read_used, &socket->context.read_used, &socket->context.session, &http_prerequest_parser_settings, &request);
    switch(state) {
      case http_incomplete: return;
      case http_valid: {
        const char save = request.path[request.path_len];
        request.path[request.path_len] = 0;
        socket->context.entry = http_hash_table_find(server->context.table, request.path);
        request.path[request.path_len] = save;
        if(socket->context.entry != NULL) {
          if(socket->context.entry->data != NULL) {
            socket->context.settings = socket->context.entry->data;
          } else {
            socket->context.settings = server->context.settings;
          }
        } else {
          /* We can't reject the request here without having the whole message
          in the read buffer, because then we would try to parse the rest of the
          message as a new request, while it was continuation of this one. What
          we can do though is set a flag that will tell the parser to not process
          the message in any way - if body is compressed, don't decompress it. If
          it's chunked, don't merge chunks together. */
          socket->context.backup_settings = *server->context.settings;
          socket->context.backup_settings.no_processing = 1;
          socket->context.backup_settings.no_character_validation = 1;
          socket->context.settings = &socket->context.backup_settings;
        }
        /* Parse from the beginning in case max_method_len or max_path_len changed */
        socket->context.session = (struct http_parser_session){0};
        socket->context.init_parsed = 1;
        break;
      }
      default: goto res;
    }
  }
  while(1) {
    state = http1_parse_request(socket->context.read_buffer, socket->context.read_used, &socket->context.read_used, &socket->context.session, socket->context.settings, &request);
    if(state == http_out_of_memory) {
      if(server->callbacks->onnomem(server) == 0) {
        continue;
      }
      goto err;
    }
    break;
  }
  if(state != http_incomplete) {
    (void) time_manager_cancel_timeout(server->context.manager, &socket->context.timeout);
    res:;
    const struct http_header* connection = NULL;
    int close_connection = 0;
    if(socket->context.session.parsed_headers) {
      connection = http1_seek_header(&request, "connection", 10);
      if(connection != NULL) {
        connection->value[connection->value_len] = 0;
        if(connection->value_len == 5 && strncasecmp(connection->value, "close", 5) == 0) {
          close_connection = 1;
        }
      }
    } else {
      close_connection = 1;
    }
    socket->context.session = (struct http_parser_session){0};
    struct http_header res_headers[255];
    struct http_message response = {0};
    response.headers = res_headers;
    switch(state) {
      case http_valid: {
        if(socket->context.entry == NULL) {
          response.status_code = http_s_not_found;
          response.reason_phrase = http1_default_reason_phrase(response.status_code);
          response.reason_phrase_len = http1_default_reason_phrase_len(response.status_code);
        } else {
          ((void (*)(struct http_server*, struct http_serversock*, struct http_message*, struct http_message*)) socket->context.entry->func)(server, socket, &request, &response);
        }
        break;
      }
      case http_body_too_long: {
        /* If Expect header is after Content-Length, we will never know about it.
        Well... non-compliant, but that's a little bit dumb. Let's just send
        http_s_payload_too_large. It means pretty much the same thing. */
        response.status_code = http_s_payload_too_large;
        response.reason_phrase = http1_default_reason_phrase(response.status_code);
        response.reason_phrase_len = http1_default_reason_phrase_len(response.status_code);
        break;
      }
      case http_path_too_long: {
        response.status_code = http_s_request_uri_too_long;
        response.reason_phrase = http1_default_reason_phrase(response.status_code);
        response.reason_phrase_len = http1_default_reason_phrase_len(response.status_code);
        break;
      }
      case http_invalid_version: {
        response.status_code = http_s_http_version_not_supported;
        response.reason_phrase = http1_default_reason_phrase(response.status_code);
        response.reason_phrase_len = http1_default_reason_phrase_len(response.status_code);
        break;
      }
      case http_header_name_too_long: {
        response.status_code = http_s_request_header_fields_too_large;
        response.reason_phrase = "Request Header Name Too Large";
        response.reason_phrase_len = 29;
        break;
      }
      case http_header_value_too_long: {
        response.status_code = http_s_request_header_fields_too_large;
        response.reason_phrase = "Request Header Value Too Large";
        response.reason_phrase_len = 30;
        break;
      }
      case http_too_many_headers: {
        response.status_code = http_s_request_header_fields_too_large;
        response.reason_phrase = "Request Has Too Many Headers";
        response.reason_phrase_len = 28;
        break;
      }
      case http_method_too_long: {
        response.status_code = http_s_bad_request;
        response.reason_phrase = "Request Method Too Large";
        response.reason_phrase_len = 24;
        break;
      }
      case http_transfer_not_supported: {
        response.status_code = http_s_not_implemented;
        response.reason_phrase = "Transfer-Encoding Not Implemented";
        response.reason_phrase_len = 33;
        break;
      }
      case http_encoding_not_supported: {
        response.status_code = http_s_not_implemented;
        response.reason_phrase = "Content-Encoding Not Implemented";
        response.reason_phrase_len = 32;
        break;
      }
      case http_corrupted_body_compression: {
        response.status_code = http_s_bad_request;
        response.reason_phrase = "Malformed Body Compression";
        response.reason_phrase_len = 26;
        break;
      }
      default: {
        response.status_code = http_s_bad_request;
        response.reason_phrase = http1_default_reason_phrase(response.status_code);
        response.reason_phrase_len = http1_default_reason_phrase_len(response.status_code);
        break;
      }
    }
    int freed_headers = 0;
    if(response.status_code != 0) {
      /* We have a response to send. Automatically attach some headers. */
#define add_header(a,b,c,d) response.headers[response.headers_len++]=(struct http_header){a,c,b,0,d,0}
      if(connection != NULL) {
        if(close_connection == 1) {
          add_header("Connection", 10, "close", 5);
        } else if(connection->value_len == 10 && strncasecmp(connection->value, "keep-alive", 10) == 0) {
          add_header("Connection", 10, "keep-alive", 10);
        } /* DO NOT automatically generate upgrade header */
      } else {
        add_header("Connection", 10, "keep-alive", 10);
      }
      if(response.encoding != http_e_none) {
        void* body = NULL;
        size_t body_len;
        while(1) {
#define pick(a,b) ((a)?(a):(b))
          switch(response.encoding) {
            case http_e_gzip: {
              z_stream s = {0};
              z_stream* const stream = gzipper(&s,
                pick(server->context.quality, Z_DEFAULT_COMPRESSION),
                pick(server->context.window_bits, Z_GZIP_DEFAULT_WINDOW_BITS),
                pick(server->context.mem_level, Z_DEFAULT_MEM_LEVEL),
                pick(server->context.mode, Z_DEFAULT_STRATEGY)
              );
              if(stream == NULL) {
                break;
              }
              body = gzip(stream, response.body, response.body_len, NULL, &body_len, Z_FINISH);
              deflateEnd(stream);
              break;
            }
            case http_e_deflate: {
              z_stream s = {0};
              z_stream* const stream = deflater(&s,
                pick(server->context.quality, Z_DEFAULT_COMPRESSION),
                pick(server->context.window_bits, Z_DEFLATE_DEFAULT_WINDOW_BITS),
                pick(server->context.mem_level, Z_DEFAULT_MEM_LEVEL),
                pick(server->context.mode, Z_DEFAULT_STRATEGY)
              );
              if(stream == NULL) {
                break;
              }
              body = deflate_(stream, response.body, response.body_len, NULL, &body_len, Z_FINISH);
              deflateEnd(stream);
              break;
            }
            case http_e_brotli: {
              body = brotli_compress2(response.body, response.body_len, &body_len,
                pick(server->context.quality, BROTLI_DEFAULT_QUALITY),
                pick(server->context.window_bits, BROTLI_DEFAULT_WINDOW),
                pick(server->context.mode, BROTLI_MODE_GENERIC));
              break;
            }
            default: __builtin_unreachable();
          }
#undef pick
          if(body == NULL) {
            if(errno == EINVAL) {
              /* Application messed up */
              if(server->context.compression_is_required) {
                /* Awkward */
                goto err_res;
              }
              goto out;
            }
            if(server->callbacks->onnomem(server) == 0) {
              continue;
            }
            if(server->context.compression_is_required) {
              goto err_res;
            }
            goto out;
          }
        }
        if(response.allocated_body) {
          free(response.body);
        } else {
          response.allocated_body = 1;
        }
        void* const ptr = realloc(body, body_len);
        if(ptr != NULL) {
          response.body = ptr;
          response.body_len = body_len;
        } else {
          response.body = body;
        }
        switch(response.encoding) {
          case http_e_gzip: {
            add_header("Content-Encoding", 16, "gzip", 4);
            break;
          }
          case http_e_deflate: {
            add_header("Content-Encoding", 16, "deflate", 7);
            break;
          }
          case http_e_brotli: {
            add_header("Content-Encoding", 16, "br", 2);
            break;
          }
          default: __builtin_unreachable();
        }
      }
      out:
      server->context.compression_is_required = 0;
      server->context.quality = 0;
      server->context.window_bits = 0;
      server->context.mem_level = 0;
      server->context.mode = 0;
      
      char content_length[11];
      const int len = sprintf(content_length, "%lu", response.body_len);
      if(len < 0) {
        goto err_res;
      }
      add_header("Content-Length", 14, content_length, len);
      
      char date_header[30];
      date_header[0] = 0;
      struct tm tms;
      if(gmtime_r(&(time_t){time(NULL)}, &tms) != NULL && strftime(date_header, 30, "%a, %d %b %Y %H:%M:%S GMT", &tms) != 0) {
        add_header("Date", 4, date_header, 29);
      }
      add_header("Server", 6, "shnet", 5);
#undef add_header
      http_serversock_send(socket, &response);
      freed_headers = 1;
      for(uint32_t i = 0; i < response.headers_len; ++i) {
        if(response.headers[i].allocated_name) {
          free(response.headers[i].name);
        }
        if(response.headers[i].allocated_value) {
          free(response.headers[i].value);
        }
      }
    }
    free(socket->context.read_buffer);
    socket->context.read_buffer = NULL;
    socket->context.read_used = 0;
    socket->context.read_size = 0;
    socket->context.init_parsed = 0;
    if(close_connection == 0) {
      return;
    }
    
    err_res:
    if(response.allocated_body) {
      free(response.body);
    }
    if(freed_headers == 0) {
      for(uint32_t i = 0; i < response.headers_len; ++i) {
        if(response.headers[i].allocated_name) {
          free(response.headers[i].name);
        }
        if(response.headers[i].allocated_value) {
          free(response.headers[i].value);
        }
      }
    }
    goto err;
  }
  return;
  
  err:
  socket->context.expected = 1;
  (void) time_manager_cancel_timeout(server->context.manager, &socket->context.timeout);
  if(socket->context.read_buffer != NULL) {
    free(socket->context.read_buffer);
    socket->context.read_buffer = NULL;
    socket->context.read_used = 0;
    socket->context.read_size = 0;
  }
  tcp_socket_force_close(&socket->tcp);
  return;
  
  websocket:
  while(1) {
    while(1) {
      state = websocket_parse(socket->context.read_buffer, socket->context.read_used, &socket->context.read_used, &socket->context);
      if(state == http_out_of_memory) {
        if(server->callbacks->onnomem(server) == 0) {
          continue;
        }
        goto errr;
      }
      break;
    }
    if(state != http_incomplete) {
      socket->context.session = (struct http_parser_session){0};
      if(state == http_valid) {
        if(socket->context.message.opcode < 8) {
          if(socket->context.message.body_len > 0) {
            socket->callbacks->onmessage(socket, socket->context.message.body, socket->context.message.body_len);
          }
          if(socket->context.message.allocated_body) {
            free(socket->context.message.body);
          }
        } else if(socket->context.message.opcode == 8) {
          if(socket->context.closing) {
            goto errr;
          }
          socket->context.closing = 1;
          if(socket->context.close_onreadclose) {
            (void) ws_send(socket, socket->context.message.body, socket->context.message.body_len > 1 ? 2 : 0, websocket_close, 0);
            tcp_socket_close(&socket->tcp);
          } else if(socket->callbacks->onreadclose != NULL) {
            socket->callbacks->onreadclose(socket, socket->context.message.close_code);
          }
        } else if(socket->context.message.opcode == 9) {
          (void) ws_send(socket, NULL, 0, websocket_pong, 0);
        }
        if(!socket->context.message.allocated_body) {
          socket->context.read_used -= socket->context.message.body_len;
        }
        if(socket->context.read_used != 0) {
          if(!socket->context.message.allocated_body) {
            (void) memmove(socket->context.read_buffer, socket->context.read_buffer + socket->context.message.body_len, socket->context.read_used);
          }
          char* const buf = realloc(socket->context.read_buffer, socket->context.read_used);
          if(buf != NULL) {
            socket->context.read_buffer = buf;
            socket->context.read_size = socket->context.read_used;
          }
        } else {
          free(socket->context.read_buffer);
          socket->context.read_buffer = NULL;
          socket->context.read_size = 0;
        }
        socket->context.message = (struct http_message){0};
      } else {
        errr:
        free(socket->context.read_buffer);
        socket->context.read_buffer = NULL;
        socket->context.read_used = 0;
        socket->context.read_size = 0;
        socket->callbacks->onclose(socket, socket->context.message.close_code);
        socket->context.message = (struct http_message){0};
        tcp_socket_force_close(&socket->tcp);
        break;
      }
    } else {
      break;
    }
  }
}

static int http_serversock_onnomem(struct tcp_socket* soc) {
  return server->callbacks->onnomem(server);
}

static void http_serversock_onclose(struct tcp_socket* soc) {
  if(!socket->context.expected && socket->context.timeout.timeout != NULL) {
    (void) time_manager_cancel_timeout(server->context.manager, &socket->context.timeout);
  }
  tcp_socket_free(&socket->tcp);
}

static void http_serversock_onfree(struct tcp_socket* soc) {
  if(socket->context.read_buffer != NULL) {
    free(socket->context.read_buffer);
  }
  if(socket->context.alloc_compressor) {
    deflater_free(socket->context.compressor);
  }
  if(socket->context.alloc_decompressor) {
    deflater_free(socket->context.decompressor);
  }
  (void) memset(socket + 1, 0, sizeof(struct http_serversock) - sizeof(struct tcp_socket));
}

#undef server
#undef socket



#define socket ((struct https_serversock*) data)
#define server ((struct http_server*) socket->tls.tcp.server)

static void https_serversock_stop_socket(void* data) {
  socket->context.expected = 1;
  tls_socket_stop_receiving_data(&socket->tls);
  char* msg;
  uint32_t len;
  if(http_serversock_timeout((struct http_serversock*) socket, &msg, &len) == 0) {
    (void) tls_send(&socket->tls, msg, len);
    free(msg);
  }
  tcp_socket_linger(&socket->tls.tcp, 5);
  tls_socket_close(&socket->tls);
}

#undef server
#undef socket

static void https_server_free(struct https_server* const server) {
  if(server->context.allocated_epoll) {
    net_epoll_stop(server->tls.tcp.epoll);
    net_epoll_free(server->tls.tcp.epoll);
    free(server->tls.tcp.epoll);
  }
  if(server->context.allocated_manager) {
    time_manager_stop(server->context.manager);
    time_manager_free(server->context.manager);
    free(server->context.manager);
  }
  if(server->context.allocated_ctx) {
    SSL_CTX_free(server->tls.ctx);
  }
  if(server->context.allocated_table) {
    http_hash_table_free(server->context.table);
    free(server->context.table);
  }
}

#define server ((struct https_server*) serv)

static int https_server_onnomem(struct tls_server* serv) {
  return server->callbacks->onnomem(server);
}

static void https_server_onshutdown(struct tls_server* serv) {
  server->callbacks->onshutdown(server);
  https_server_free(server);
  tls_server_free(&server->tls);
  free(server);
}

#undef server

#define server ((struct https_server*) socket->tls.tcp.server)

static void https_serversock_send(struct https_serversock* const socket, struct http_message* const message) {
  char* msg;
  const uint32_t len = http1_message_length(message);
  while(1) {
    msg = malloc(len);
    if(msg == NULL) {
      if(server->callbacks->onnomem(server) == 0) {
        continue;
      }
      (void) time_manager_cancel_timeout(server->context.manager, &socket->context.timeout);
      if(message->allocated_body) {
        free(message->body);
      }
      free(socket->tls.read_buffer);
      socket->tls.read_buffer = NULL;
      tcp_socket_force_close(&socket->tls.tcp);
      return;
    }
    break;
  }
  http1_create_message(msg, message);
  (void) tls_send(&socket->tls, msg, len);
  free(msg);
}

#undef server

#define socket ((struct https_serversock*) soc)
#define server ((struct https_server*) soc->tcp.server)

static int https_server_onconnection(struct tls_socket* soc) {
  socket->tls.tcp.settings = server->context.tcp_settings;
  socket->tls.callbacks = &https_serversock_callbacks;
  return 0;
}

static void https_serversock_onopen(struct tls_socket* soc) {
  if(server->context.timeout_after != UINT64_MAX) {
    while(1) {
      const int err = time_manager_add_timeout(server->context.manager, time_get_sec(server->context.timeout_after), https_serversock_stop_socket, socket, &socket->context.timeout);
      if(err != 0) {
        if(server->callbacks->onnomem(server) == 0) {
          continue;
        }
        tcp_socket_force_close(&socket->tls.tcp);
        return;
      }
      break;
    }
  }
}

static void https_serversock_onmessage(struct tls_socket* soc) {
  if(socket->context.protocol == http_p_websocket) {
    goto websocket;
  }
  struct http_header req_headers[255];
  struct http_message request = {0};
  request.headers = req_headers;
  request.headers_len = 255;
  int state;
  if(socket->context.init_parsed == 0) {
    state = http1_parse_request(socket->tls.read_buffer, socket->tls.read_used, &socket->tls.read_used, &socket->context.session, &http_prerequest_parser_settings, &request);
    switch(state) {
      case http_incomplete: return;
      case http_valid: {
        const char save = request.path[request.path_len];
        request.path[request.path_len] = 0;
        socket->context.entry = http_hash_table_find(server->context.table, request.path);
        request.path[request.path_len] = save;
        if(socket->context.entry != NULL) {
          if(socket->context.entry->data != NULL) {
            socket->context.settings = socket->context.entry->data;
          } else {
            socket->context.settings = server->context.settings;
          }
        } else {
          socket->context.backup_settings = *server->context.settings;
          socket->context.backup_settings.no_processing = 1;
          socket->context.backup_settings.no_character_validation = 1;
          socket->context.settings = &socket->context.backup_settings;
        }
        socket->context.session = (struct http_parser_session){0};
        socket->context.init_parsed = 1;
        break;
      }
      default: goto res;
    }
  }
  while(1) {
    state = http1_parse_request(socket->tls.read_buffer, socket->tls.read_used, &socket->tls.read_used, &socket->context.session, socket->context.settings, &request);
    if(state == http_out_of_memory) {
      if(server->callbacks->onnomem(server) == 0) {
        continue;
      }
      goto err;
    }
    break;
  }
  if(state != http_incomplete) {
    (void) time_manager_cancel_timeout(server->context.manager, &socket->context.timeout);
    res:;
    const struct http_header* connection = NULL;
    int close_connection = 0;
    if(socket->context.session.parsed_headers) {
      connection = http1_seek_header(&request, "connection", 10);
      if(connection != NULL) {
        connection->value[connection->value_len] = 0;
        if(connection->value_len == 5 && strncasecmp(connection->value, "close", 5) == 0) {
          close_connection = 1;
        }
      }
    } else {
      close_connection = 1;
    }
    socket->context.session = (struct http_parser_session){0};
    struct http_header res_headers[255];
    struct http_message response = {0};
    response.headers = res_headers;
    switch(state) {
      case http_valid: {
        if(socket->context.entry == NULL) {
          response.status_code = http_s_not_found;
          response.reason_phrase = http1_default_reason_phrase(response.status_code);
          response.reason_phrase_len = http1_default_reason_phrase_len(response.status_code);
        } else {
          ((void (*)(struct https_server*, struct https_serversock*, struct http_message*, struct http_message*)) socket->context.entry->func)(server, socket, &request, &response);
        }
        break;
      }
      case http_body_too_long: {
        response.status_code = http_s_payload_too_large;
        response.reason_phrase = http1_default_reason_phrase(response.status_code);
        response.reason_phrase_len = http1_default_reason_phrase_len(response.status_code);
        break;
      }
      case http_path_too_long: {
        response.status_code = http_s_request_uri_too_long;
        response.reason_phrase = http1_default_reason_phrase(response.status_code);
        response.reason_phrase_len = http1_default_reason_phrase_len(response.status_code);
        break;
      }
      case http_invalid_version: {
        response.status_code = http_s_http_version_not_supported;
        response.reason_phrase = http1_default_reason_phrase(response.status_code);
        response.reason_phrase_len = http1_default_reason_phrase_len(response.status_code);
        break;
      }
      case http_header_name_too_long: {
        response.status_code = http_s_request_header_fields_too_large;
        response.reason_phrase = "Request Header Name Too Large";
        response.reason_phrase_len = 29;
        break;
      }
      case http_header_value_too_long: {
        response.status_code = http_s_request_header_fields_too_large;
        response.reason_phrase = "Request Header Value Too Large";
        response.reason_phrase_len = 30;
        break;
      }
      case http_too_many_headers: {
        response.status_code = http_s_request_header_fields_too_large;
        response.reason_phrase = "Request Has Too Many Headers";
        response.reason_phrase_len = 28;
        break;
      }
      case http_method_too_long: {
        response.status_code = http_s_bad_request;
        response.reason_phrase = "Request Method Too Large";
        response.reason_phrase_len = 24;
        break;
      }
      case http_transfer_not_supported: {
        response.status_code = http_s_not_implemented;
        response.reason_phrase = "Transfer-Encoding Not Implemented";
        response.reason_phrase_len = 33;
        break;
      }
      case http_encoding_not_supported: {
        response.status_code = http_s_not_implemented;
        response.reason_phrase = "Content-Encoding Not Implemented";
        response.reason_phrase_len = 32;
        break;
      }
      case http_corrupted_body_compression: {
        response.status_code = http_s_bad_request;
        response.reason_phrase = "Malformed Body Compression";
        response.reason_phrase_len = 26;
        break;
      }
      default: {
        response.status_code = http_s_bad_request;
        response.reason_phrase = http1_default_reason_phrase(response.status_code);
        response.reason_phrase_len = http1_default_reason_phrase_len(response.status_code);
        break;
      }
    }
    int freed_headers = 0;
    if(response.status_code != 0) {
#define add_header(a,b,c,d) response.headers[response.headers_len++]=(struct http_header){a,c,b,0,d,0}
      if(connection != NULL) {
        if(close_connection == 1) {
          add_header("Connection", 10, "close", 5);
        } else if(connection->value_len == 10 && strncasecmp(connection->value, "keep-alive", 10) == 0) {
          add_header("Connection", 10, "keep-alive", 10);
        }
      } else {
        add_header("Connection", 10, "keep-alive", 10);
      }
      if(response.encoding != http_e_none) {
        void* body = NULL;
        size_t body_len;
        while(1) {
#define pick(a,b) ((a)?(a):(b))
          switch(response.encoding) {
            case http_e_gzip: {
              z_stream s = {0};
              z_stream* const stream = gzipper(&s,
                pick(server->context.quality, Z_DEFAULT_COMPRESSION),
                pick(server->context.window_bits, Z_GZIP_DEFAULT_WINDOW_BITS),
                pick(server->context.mem_level, Z_DEFAULT_MEM_LEVEL),
                pick(server->context.mode, Z_DEFAULT_STRATEGY)
              );
              if(stream == NULL) {
                break;
              }
              body = gzip(stream, response.body, response.body_len, NULL, &body_len, Z_FINISH);
              deflateEnd(stream);
              break;
            }
            case http_e_deflate: {
              z_stream s = {0};
              z_stream* const stream = deflater(&s,
                pick(server->context.quality, Z_DEFAULT_COMPRESSION),
                pick(server->context.window_bits, Z_DEFLATE_DEFAULT_WINDOW_BITS),
                pick(server->context.mem_level, Z_DEFAULT_MEM_LEVEL),
                pick(server->context.mode, Z_DEFAULT_STRATEGY)
              );
              if(stream == NULL) {
                break;
              }
              body = deflate_(stream, response.body, response.body_len, NULL, &body_len, Z_FINISH);
              deflateEnd(stream);
              break;
            }
            case http_e_brotli: {
              body = brotli_compress2(response.body, response.body_len, &body_len,
                pick(server->context.quality, BROTLI_DEFAULT_QUALITY),
                pick(server->context.window_bits, BROTLI_DEFAULT_WINDOW),
                pick(server->context.mode, BROTLI_MODE_GENERIC));
              break;
            }
            default: __builtin_unreachable();
          }
#undef pick
          if(body == NULL) {
            if(errno == EINVAL) {
              if(server->context.compression_is_required) {
                goto err_res;
              }
              goto out;
            }
            if(server->callbacks->onnomem(server) == 0) {
              continue;
            }
            if(server->context.compression_is_required) {
              goto err_res;
            }
            goto out;
          }
        }
        if(response.allocated_body) {
          free(response.body);
        } else {
          response.allocated_body = 1;
        }
        void* const ptr = realloc(body, body_len);
        if(ptr != NULL) {
          response.body = ptr;
          response.body_len = body_len;
        } else {
          response.body = body;
        }
        switch(response.encoding) {
          case http_e_gzip: {
            add_header("Content-Encoding", 16, "gzip", 4);
            break;
          }
          case http_e_deflate: {
            add_header("Content-Encoding", 16, "deflate", 7);
            break;
          }
          case http_e_brotli: {
            add_header("Content-Encoding", 16, "br", 2);
            break;
          }
          default: __builtin_unreachable();
        }
      }
      out:
      server->context.compression_is_required = 0;
      server->context.quality = 0;
      server->context.window_bits = 0;
      server->context.mem_level = 0;
      server->context.mode = 0;
      
      char content_length[11];
      const int len = sprintf(content_length, "%lu", response.body_len);
      if(len < 0) {
        goto err_res;
      }
      add_header("Content-Length", 14, content_length, len);
      
      char date_header[30];
      date_header[0] = 0;
      struct tm tms;
      if(gmtime_r(&(time_t){time(NULL)}, &tms) != NULL && strftime(date_header, 30, "%a, %d %b %Y %H:%M:%S GMT", &tms) != 0) {
        add_header("Date", 4, date_header, 29);
      }
      add_header("Server", 6, "shnet", 5);
#undef add_header
      https_serversock_send(socket, &response);
      freed_headers = 1;
      for(uint32_t i = 0; i < response.headers_len; ++i) {
        if(response.headers[i].allocated_name) {
          free(response.headers[i].name);
        }
        if(response.headers[i].allocated_value) {
          free(response.headers[i].value);
        }
      }
    }
    free(socket->tls.read_buffer);
    socket->tls.read_buffer = NULL;
    socket->tls.read_used = 0;
    socket->tls.read_size = 0;
    socket->context.init_parsed = 0;
    if(close_connection == 0) {
      return;
    }
    
    err_res:
    if(response.allocated_body) {
      free(response.body);
    }
    if(freed_headers == 0) {
      for(uint32_t i = 0; i < response.headers_len; ++i) {
        if(response.headers[i].allocated_name) {
          free(response.headers[i].name);
        }
        if(response.headers[i].allocated_value) {
          free(response.headers[i].value);
        }
      }
    }
    goto err;
  }
  return;
  
  err:
  socket->context.expected = 1;
  (void) time_manager_cancel_timeout(server->context.manager, &socket->context.timeout);
  if(socket->tls.read_buffer != NULL) {
    free(socket->tls.read_buffer);
    socket->tls.read_buffer = NULL;
    socket->tls.read_used = 0;
    socket->tls.read_size = 0;
  }
  tcp_socket_force_close(&socket->tls.tcp);
  return;
  
  
  websocket:
  while(1) {
    while(1) {
      state = websocket_parse(socket->tls.read_buffer, socket->tls.read_used, &socket->tls.read_used, &socket->context);
      if(state == http_out_of_memory) {
        if(server->callbacks->onnomem(server) == 0) {
          continue;
        }
        goto errr;
      }
      break;
    }
    if(state != http_incomplete) {
      socket->context.session = (struct http_parser_session){0};
      if(state == http_valid) {
        if(socket->context.message.opcode < 8) {
          if(socket->context.message.body_len > 0) {
            socket->callbacks->onmessage(socket, socket->context.message.body, socket->context.message.body_len);
          }
          if(socket->context.message.allocated_body) {
            free(socket->context.message.body);
          }
        } else if(socket->context.message.opcode == 8) {
          if(socket->context.closing) {
            goto errr;
          }
          socket->context.closing = 1;
          if(socket->context.close_onreadclose) {
            (void) ws_send(socket, socket->context.message.body, socket->context.message.body_len > 1 ? 2 : 0, websocket_close, 0);
            tcp_socket_close(&socket->tls.tcp);
          } else if(socket->callbacks->onreadclose != NULL) {
            socket->callbacks->onreadclose(socket, socket->context.message.close_code);
          }
        } else if(socket->context.message.opcode == 9) {
          (void) ws_send(socket, NULL, 0, websocket_pong, 0);
        }
        if(!socket->context.message.allocated_body) {
          socket->tls.read_used -= socket->context.message.body_len;
        }
        if(socket->tls.read_used != 0) {
          if(!socket->context.message.allocated_body) {
            (void) memmove(socket->tls.read_buffer, socket->tls.read_buffer + socket->context.message.body_len, socket->tls.read_used);
          }
          char* const buf = realloc(socket->tls.read_buffer, socket->tls.read_used);
          if(buf != NULL) {
            socket->tls.read_buffer = buf;
            socket->tls.read_size = socket->tls.read_used;
          }
        } else {
          free(socket->tls.read_buffer);
          socket->tls.read_buffer = NULL;
          socket->tls.read_size = 0;
        }
        socket->context.message = (struct http_message){0};
      } else {
        errr:
        free(socket->tls.read_buffer);
        socket->tls.read_buffer = NULL;
        socket->tls.read_used = 0;
        socket->tls.read_size = 0;
        socket->callbacks->onclose(socket, socket->context.message.close_code);
        socket->context.message = (struct http_message){0};
        tcp_socket_force_close(&socket->tls.tcp);
        break;
      }
    } else {
      break;
    }
  }
}

static int https_serversock_onnomem(struct tls_socket* soc) {
  return server->callbacks->onnomem(server);
}

static void https_serversock_onclose(struct tls_socket* soc) {
  if(!socket->context.expected && socket->context.timeout.timeout != NULL) {
    (void) time_manager_cancel_timeout(server->context.manager, &socket->context.timeout);
  }
  tls_socket_free(&socket->tls);
}

static void https_serversock_onfree(struct tls_socket* soc) {
  if(socket->context.read_buffer != NULL) {
    free(socket->context.read_buffer);
  }
  if(socket->context.alloc_compressor) {
    deflater_free(socket->context.compressor);
  }
  if(socket->context.alloc_decompressor) {
    deflater_free(socket->context.decompressor);
  }
  (void) memset(socket + 1, 0, sizeof(struct https_serversock) - sizeof(struct tls_socket));
}

#undef server
#undef socket



int http_server(char* const str, struct http_server_options* options) {
  struct http_uri uri = http_parse_uri(str);
  struct http_server_options opt = {0};
  if(options == NULL) {
    options = &opt;
  }
  if(uri.secure) {
    if(https_setup_server(&uri, options) != 0) {
      return -1;
    }
  } else {
    if(http_setup_server(&uri, options) != 0) {
      return -1;
    }
  }
  return 0;
}



int ws(void* const _base, const struct http_parser_settings* const settings, const struct http_message* const request, struct http_message* const response, const int up) {
  struct http_serversock_context* context;
  if(((struct net_socket_base*) _base)->which & net_secure) {
    context = (struct http_serversock_context*)((char*) _base + sizeof(struct tls_socket));
  } else {
    context = (struct http_serversock_context*)((char*) _base + sizeof(struct tcp_socket));
  }
  const struct http_header* const connection = http1_seek_header(request, "connection", 10);
  if(connection == NULL || strstr(connection->value, "pgrade") == NULL) {
    return 0;
  }
  const struct http_header* const upgrade = http1_seek_header(request, "upgrade", 7);
  if(upgrade == NULL || upgrade->value_len != 9 || strncasecmp(upgrade->value, "websocket", 9) != 0) {
    return 0;
  }
  const struct http_header* const sec_version = http1_seek_header(request, "sec-websocket-version", 21);
  if(sec_version == NULL || sec_version->value_len != 2 || memcmp(sec_version->value, "13", 2) != 0) {
    return 0;
  }
  const struct http_header* const sec_key = http1_seek_header(request, "sec-websocket-key", 17);
  if(sec_key == NULL || sec_key->value_len != 24) {
    return 0;
  }
  if(up) {
    unsigned char result[SHA_DIGEST_LENGTH];
    EVP_MD_CTX* ctx = EVP_MD_CTX_new();
    if(ctx == NULL) {
      return -1;
    }
    if(EVP_DigestInit_ex(ctx, EVP_sha1(), NULL) == 0 ||
       EVP_DigestUpdate(ctx, sec_key->value, 24) == 0 ||
       EVP_DigestUpdate(ctx, "258EAFA5-E914-47DA-95CA-C5AB0DC85B11", 36) == 0 ||
       EVP_DigestFinal_ex(ctx, result, NULL) == 0) {
      EVP_MD_CTX_free(ctx);
      return -1;
    }
    EVP_MD_CTX_free(ctx);
    char* const h = base64_encode(result, NULL, SHA_DIGEST_LENGTH);
    if(h == NULL) {
      return -1;
    }
    if(!settings->no_permessage_deflate) {
      const struct http_header* const deflate = http1_seek_header(request, "sec-websocket-extensions", 24);
      if(deflate != NULL) {
        const char save = deflate->value[deflate->value_len];
        deflate->value[deflate->value_len] = 0;
        context->permessage_deflate = strstr(deflate->value, "permessage-deflate") != NULL;
        deflate->value[deflate->value_len] = save;
      }
    }
    response->headers[response->headers_len++] = (struct http_header){"Connection","Upgrade",10,0,7,0};
    response->headers[response->headers_len++] = (struct http_header){"Upgrade","websocket",7,0,9,0};
    response->headers[response->headers_len++] = (struct http_header){"Sec-WebSocket-Accept",h,20,0,28,1};
    if(context->permessage_deflate) {
      response->headers[response->headers_len++] = (struct http_header){"Sec-WebSocket-Extensions","permessage-deflate",24,0,18,0};
    }
    response->status_code = http_s_switching_protocols;
    response->reason_phrase = http1_default_reason_phrase(response->status_code);
    response->reason_phrase_len = http1_default_reason_phrase_len(response->status_code);
    context->session = (struct http_parser_session){0};
    context->protocol = http_p_websocket;
    if(((struct net_socket_base*) _base)->which & net_secure) {
      (void) time_manager_cancel_timeout(((struct https_server*)((struct https_serversock*) _base)->tls.tcp.server)->context.manager, &context->timeout);
    } else {
      (void) time_manager_cancel_timeout(((struct http_server*)((struct http_serversock*) _base)->tcp.server)->context.manager, &context->timeout);
    }
  }
  return 1;
}

#define ATLEAST(a) \
do { \
  if((a) > size) return http_incomplete; \
} while(0)

int websocket_parse(void* _buffer, uint64_t size, uint64_t* const used, struct http_serversock_context* const context) {
  unsigned char* buffer = _buffer;
  ATLEAST(2);
  const uint32_t fin = buffer[0] & 128;
  if(fin) {
    context->message.transfer = 1;
  }
  const uint32_t compressed = buffer[0] & 64;
  if(compressed) {
    if(!context->permessage_deflate) {
      return http_invalid;
    }
    context->message.encoding = http_e_deflate;
  }
  if(buffer[0] & 48) {
    return http_invalid;
  }
  const uint32_t opcode = buffer[0] & 15;
  if((opcode > 2 && opcode < 8) || opcode > 10) {
    return http_invalid;
  }
  if(opcode == 0 && context->message.transfer && context->message.body != NULL) {
    return http_invalid;
  }
  if((opcode & 8) && !context->message.transfer) {
    return http_invalid;
  }
  context->message.opcode = opcode;
  ++buffer;
  const uint32_t has_mask = (buffer[0] & 128) != 0;
  if(!context->settings->client) {
    if(has_mask == 0) {
      return http_invalid;
    }
  } else if(has_mask == 1) {
    return http_invalid;
  }
  uint64_t payload_len = buffer[0] & 127;
  ++buffer;
  const uint32_t header_len = (payload_len == 126 ? 2 : payload_len == 127 ? 8 : 0) + (has_mask << 2) + 2;
  ATLEAST(header_len);
  if(payload_len == 126) {
    payload_len = (buffer[0] << 8) | buffer[1];
    buffer += 2;
    size -= 4;
  } else if(payload_len != 127) {
    size -= 2;
  } else {
    payload_len = ((uint64_t) buffer[0] << 56) | ((uint64_t) buffer[1] << 48) | ((uint64_t) buffer[2] << 40) | ((uint64_t) buffer[3] << 32)
                | ((uint64_t) buffer[4] << 24) | ((uint64_t) buffer[5] << 16) | ((uint64_t) buffer[6] << 8 ) |  (uint64_t) buffer[7];
    buffer += 8;
    size -= 10;
  }
  if((opcode & 8) && payload_len > 125) {
    return http_invalid;
  }
  context->message.body_len += payload_len;
  if(payload_len > context->settings->max_body_len || context->message.body_len > context->settings->max_body_len) {
    return http_body_too_long;
  }
  uint8_t mask[4];
  if(has_mask) {
    (void) memcpy(mask, buffer, 4);
    buffer += 4;
    size -= 4;
  }
  ATLEAST(payload_len);
  buffer -= header_len;
  *used -= header_len;
  (void) memmove(buffer, buffer + header_len, *used);
  if(has_mask) {
    const uint64_t full_unmask = payload_len & ~3;
    uint64_t i = 0;
    for(; i < full_unmask; i += 4) {
      buffer[i] ^= mask[0];
      buffer[i + 1] ^= mask[1];
      buffer[i + 2] ^= mask[2];
      buffer[i + 3] ^= mask[3];
    }
    for(; i < payload_len; ++i) {
      buffer[i] ^= mask[i % 4];
    }
  }
  if(context->message.body == NULL) {
    context->message.body = (char*) buffer;
  }
  if(context->message.transfer) {
    if(context->message.encoding && context->message.body_len != 0) {
      if(context->decompressor == NULL) {
        context->decompressor = inflater(NULL, -Z_DEFLATE_DEFAULT_WINDOW_BITS);
        if(context->decompressor == NULL) {
          return http_out_of_memory;
        }
        context->alloc_decompressor = 1;
      }
      const char saves[4] = {
        context->message.body[context->message.body_len + 0],
        context->message.body[context->message.body_len + 1],
        context->message.body[context->message.body_len + 2],
        context->message.body[context->message.body_len + 3]
      };
      context->message.body[context->message.body_len + 0] = 0;
      context->message.body[context->message.body_len + 1] = 0;
      context->message.body[context->message.body_len + 2] = (char) 255;
      context->message.body[context->message.body_len + 3] = (char) 255;
      size_t len = 0;
      char* ptr = inflate_(context->decompressor, context->message.body, context->message.body_len + 4, NULL, &len, context->settings->max_body_len, Z_SYNC_FLUSH);
      context->message.body[context->message.body_len + 0] = saves[0];
      context->message.body[context->message.body_len + 1] = saves[1];
      context->message.body[context->message.body_len + 2] = saves[2];
      context->message.body[context->message.body_len + 3] = saves[3];
      if(ptr == NULL) {
        if(errno == ENOMEM) {
          return http_out_of_memory;
        } else if(errno == EOVERFLOW) {
          return http_body_too_long;
        } else {
          return http_corrupted_body_compression;
        }
      }
      *used -= context->message.body_len;
      if(*used != 0) {
        (void) memmove(context->message.body, context->message.body + context->message.body_len, *used);
      }
      context->message.body = ptr;
      ptr = realloc(context->message.body, len);
      if(ptr != NULL) {
        context->message.body = ptr;
      }
      context->message.body_len = len;
      context->message.allocated_body = 1;
    }
    if(opcode == 8 && context->message.body_len > 1) {
      context->message.close_code = ((unsigned char) context->message.body[0] << 8) | (unsigned char) context->message.body[1];
      return http_closed;
    }
    return http_valid;
  }
  return http_incomplete;
}

#undef ATLEAST

int websocket_len(const struct http_message* const message) {
  return 2 + (message->body_len > UINT16_MAX ? 8 : message->body_len > 125 ? 2 : 0) + message->body_len;
}

static uint64_t websocket_create_message_base(void* _buffer, const struct http_message* const message) {
  unsigned char* buffer = _buffer;
  if(message->transfer) {
    buffer[0] |= 128;
  }
  if(message->encoding) {
    buffer[0] |= 64;
  }
  buffer[0] |= message->opcode;
  ++buffer;
  /* if(message->client), generate and apply a mask */
  if(message->body_len > UINT16_MAX) {
    buffer[0] |= 127;
    buffer[1] =  message->body_len >> 56       ;
    buffer[2] = (message->body_len >> 48) & 255;
    buffer[3] = (message->body_len >> 40) & 255;
    buffer[4] = (message->body_len >> 32) & 255;
    buffer[5] = (message->body_len >> 24) & 255;
    buffer[6] = (message->body_len >> 16) & 255;
    buffer[7] = (message->body_len >>  8) & 255;
    buffer[8] = (message->body_len      ) & 255;
    return 10;
  } else if(message->body_len > 125) {
    buffer[0] |= 126;
    buffer[1] = message->body_len >>  8;
    buffer[2] = message->body_len & 255;
    return 4;
  } else {
    buffer[0] |= message->body_len;
    return 2;
  }
}

void websocket_create_message(void* _buffer, const struct http_message* const message) {
  unsigned char* buffer = _buffer;
  buffer += websocket_create_message_base(buffer, message);
  if(message->body != NULL) {
    (void) memcpy(buffer, message->body, message->body_len);
  }
}

int ws_send(void* const _base, void* _buffer, uint64_t size, const uint8_t opcode, const uint8_t is_not_last) {
  struct http_serversock_context* context;
  if(((struct net_socket_base*) _base)->which & net_secure) {
    context = (struct http_serversock_context*)((char*) _base + sizeof(struct tls_socket));
  } else {
    context = (struct http_serversock_context*)((char*) _base + sizeof(struct tcp_socket));
  }
  if(context->permessage_deflate && context->compressor == NULL) {
    context->compressor = deflater(NULL, Z_DEFAULT_COMPRESSION, -Z_DEFLATE_DEFAULT_WINDOW_BITS, Z_DEFAULT_MEM_LEVEL, Z_DEFAULT_STRATEGY);
    if(context->compressor == NULL) {
      return -1;
    }
    context->alloc_compressor = 1;
  }
  unsigned char* buffer = _buffer;
  unsigned char base[10] = {0};
  uint8_t alloc = 0;
  if(context->permessage_deflate) {
    size_t len = 0;
    alloc = 1;
    buffer = deflate_(context->compressor, buffer, size, NULL, &len, Z_SYNC_FLUSH);
    if(buffer == NULL) {
      return -1;
    }
    size = len - 4;
    len -= 4;
  }
  const uint64_t len = websocket_create_message_base(base, &((struct http_message) {
    .transfer = is_not_last ? 0 : 1,
    .encoding = context->permessage_deflate,
    .opcode = opcode,
    .body_len = size
  }));
  int ret;
  if(((struct net_socket_base*) _base)->which & net_secure) {
    struct https_serversock* const socket = (struct https_serversock*) _base;
    if(!tls_send(&socket->tls, base, len)) {
      return -1;
    }
    ret = tls_send(&socket->tls, buffer, size) - 1;
  } else {
    struct http_serversock* const socket = (struct http_serversock*) _base;
    if(tcp_send(&socket->tcp, base, len) != len) {
      return -1;
    }
    ret = (tcp_send(&socket->tcp, buffer, size) == size) - 1;
  }
  if(alloc) {
    free(buffer);
  }
  return ret;
}