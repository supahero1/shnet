#include "http.h"
#include "debug.h"
#include "compress.h"

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

static struct tls_socket_settings  https_client_settings = { 0, 65536, 0, 0, 0, tls_onreadclose_tls_close };

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
  .max_header_value_len = 256,
  .max_body_len = 1024000 /* ~1MB */
};

static struct http_parser_settings http_response_parser_settings = {
  .max_reason_phrase_len = 64,
  .max_headers = 32,
  .max_header_name_len = 32,
  .max_header_value_len = 1024,
  .max_body_len = 16384000 /* ~16MB */
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
    } else {
      uri.path_len = (uint32_t)(len - ((uintptr_t) uri.path - (uintptr_t) str));
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
          headers[idx++] = (struct http_header){.name=a,.name_len=b,.value=c,.value_len=d}; \
        } \
        break; \
      } \
    } \
  } \
  if(!done) { \
    headers[idx++] = (struct http_header){.name=a,.name_len=b,.value=c,.value_len=d}; \
  } \
} while(0)
    set_header("Accept", 6, "*/*", 3);
    set_header("Accept-Encoding", 15, "gzip, deflate, br", 17);
    set_header("Accept-Language", 15, "en-US,en;q=0.9", 14);
    if(opt->requests[k].no_cache) {
      set_header("Cache-Control", 13, "max-age=0", 9);
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
            body = gzip_compress2(opt->requests[k].request->body, opt->requests[k].request->body_len, &body_len,
              pick(opt->requests[k].quality, Z_DEFAULT_COMPRESSION),
              pick(opt->requests[k].window_bits, Z_GZIP_DEFAULT_WINDOW_BITS),
              pick(opt->requests[k].mem_level, Z_DEFAULT_MEM_LEVEL),
              pick(opt->requests[k].mode, Z_DEFAULT_STRATEGY));
            break;
          }
          case http_e_deflate: {
            body = deflate_compress2(opt->requests[k].request->body, opt->requests[k].request->body_len, &body_len,
              pick(opt->requests[k].quality, Z_DEFAULT_COMPRESSION),
              pick(opt->requests[k].window_bits, Z_DEFLATE_DEFAULT_WINDOW_BITS),
              pick(opt->requests[k].mem_level, Z_DEFAULT_MEM_LEVEL),
              pick(opt->requests[k].mode, Z_DEFAULT_STRATEGY));
            break;
          }
          case http_e_brotli: {
            body = brotli_compress2(opt->requests[k].request->body, opt->requests[k].request->body_len, &body_len,
              pick(opt->requests[k].quality, BROTLI_DEFAULT_QUALITY),
              pick(opt->requests[k].window_bits, BROTLI_DEFAULT_WINDOW),
              pick(opt->requests[k].mode, BROTLI_MODE_GENERIC));
            break;
          }
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
    set_header("Host", 4, uri->hostname, uri->hostname_len);
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
  if(socket->context.force_close) {
    tcp_socket_force_close(&socket->tcp);
  } else {
    tcp_socket_stop_receiving_data(&socket->tcp);
    tcp_socket_close(&socket->tcp);
  }
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
  socket->context.force_close = opt->force_close;
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
  if(socket->context.force_close) {
    tcp_socket_force_close(&socket->tls.tcp);
  } else {
    tls_socket_stop_receiving_data(&socket->tls);
    tls_socket_close(&socket->tls);
  }
}

#undef socket

static int https_setup_socket(struct http_uri* const uri, struct http_options* const opt, struct http_requests* requests) {
  struct https_socket* const socket = calloc(1, sizeof(struct https_socket));
  if(socket == NULL) {
    goto err_req;
  }
  http_init();
  socket->context.secure = 1;
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
  socket->context.force_close = opt->force_close;
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
  if(socket->context.timeout_after != UINT64_MAX) {
    while(1) {
      const int err = time_manager_add_timeout(socket->context.manager, time_get_sec(socket->context.timeout_after), http_stop_socket, socket, &socket->context.timeout);
      if(err != 0) {
        if(socket->callbacks->onnomem(socket) == 0) {
          continue;
        }
        socket->callbacks->onclose(socket, http_no_memory);
        socket->context.expected = 1;
        if(socket->context.force_close) {
          tcp_socket_force_close(&socket->tcp);
        } else {
          tcp_socket_stop_receiving_data(&socket->tcp);
          tcp_socket_close(&socket->tcp);
        }
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
        if(socket->context.force_close) {
          tcp_socket_force_close(&socket->tcp);
        } else {
          tcp_socket_stop_receiving_data(&socket->tcp);
          tcp_socket_close(&socket->tcp);
        }
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
  struct http_header headers[255];
  struct http_message response = {0};
  response.headers = headers;
  response.headers_len = 255;
  int state;
  while(1) {
    state = http1_parse_response(socket->context.read_buffer, socket->context.read_used, &socket->context.session, socket->context.requests[socket->context.requests_used].response_settings, &response);
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
        socket->context.session = (struct http_parser_session){0};
        response = (struct http_message){0};
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
  if(socket->context.force_close) {
    tcp_socket_force_close(&socket->tcp);
  } else {
    tcp_socket_stop_receiving_data(&socket->tcp);
    tcp_socket_close(&socket->tcp);
  }
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
  if(socket->context.timeout_after != UINT64_MAX) {
    while(1) {
      const int err = time_manager_add_timeout(socket->context.manager, time_get_sec(socket->context.timeout_after), https_stop_socket, socket, &socket->context.timeout);
      if(err != 0) {
        if(socket->callbacks->onnomem(socket) == 0) {
          continue;
        }
        socket->callbacks->onclose(socket, http_no_memory);
        socket->context.expected = 1;
        if(socket->context.force_close) {
          tcp_socket_force_close(&socket->tls.tcp);
        } else {
          tls_socket_stop_receiving_data(&socket->tls);
          tls_socket_close(&socket->tls);
        }
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
    state = http1_parse_response(socket->tls.read_buffer, socket->tls.read_used, &socket->context.session, socket->context.requests[socket->context.requests_used].response_settings, &response);
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
        socket->context.session = (struct http_parser_session){0};
        response = (struct http_message){0};
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
  if(socket->context.force_close) {
    tcp_socket_force_close(&socket->tls.tcp);
  } else {
    tls_socket_stop_receiving_data(&socket->tls);
    tls_socket_close(&socket->tls);
  }
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




#define http_server ((struct http_server*) socket->tcp.server)
#define https_server ((struct https_server*) socket->tcp.server)

static int http_serversock_timeout(struct http_serversock* const socket, char** const ret_msg, uint32_t* const ret_len) {
  struct http_header headers[3];
  struct http_message response = {0};
  response.headers = headers;
  
  char date_header[30];
  date_header[0] = 0;
  struct tm tms;
  if(gmtime_r(&(time_t){time(NULL)}, &tms) != NULL && strftime(date_header, 30, "%a, %d %b %Y %H:%M:%S GMT", &tms) != 0) {
    headers[0] = (struct http_header) { "Connection", "close", 10, 5 };
    headers[1] = (struct http_header) { "Date", date_header, 4, 29 };
    headers[2] = (struct http_header) { "Server", "shnet", 6, 5 };
    response.headers_len = 3;
  } else {
    headers[0] = (struct http_header) { "Connection", "close", 10, 5 };
    headers[1] = (struct http_header) { "Server", "shnet", 6, 5 };
    response.headers_len = 2;
  }
  char* msg;
  const uint32_t len = http1_message_length(&response);
  while(1) {
    msg = malloc(len);
    if(msg == NULL) {
      if(http_server->context.secure) {
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
  if(server->context.force_close) {
    tcp_socket_force_close(&socket->tcp);
  } else {
    tcp_socket_stop_receiving_data(&socket->tcp);
    char* msg;
    uint32_t len;
    if(http_serversock_timeout(socket, &msg, &len) == 0) {
      (void) tcp_send(&socket->tcp, msg, len);
    }
    tcp_socket_close(&socket->tcp);
  }
}

#undef server
#undef socket

#define socket ((struct https_serversock*) data)
#define server ((struct http_server*) socket->tls.tcp.server)

static void https_serversock_stop_socket(void* data) {
  if(server->context.force_close) {
    tcp_socket_force_close(&socket->tls.tcp);
  } else {
    tls_socket_stop_receiving_data(&socket->tls);
    char* msg;
    uint32_t len;
    if(http_serversock_timeout((struct http_serversock*) socket, &msg, &len) == 0) {
      (void) tls_send(&socket->tls, msg, len);
    }
    tls_socket_close(&socket->tls);
  }
}

#undef server
#undef socket

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
  server->context.force_close = opt->force_close;
  server->tcp.socket_size = sizeof(struct http_serversock);
  if(opt->request_settings == NULL) {
    server->context.request_settings = &http_request_parser_settings;
  } else {
    server->context.request_settings = opt->request_settings;
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
      http_hash_table_insert(server->context.table, opt->resources[i].path, (http_hash_table_value_t) opt->resources[i].http_callback);
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
  server->context.secure = 1;
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
  server->context.force_close = opt->force_close;
  server->tls.tcp.socket_size = sizeof(struct https_serversock);
  if(opt->request_settings == NULL) {
    server->context.request_settings = &http_request_parser_settings;
  } else {
    server->context.request_settings = opt->request_settings;
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
      http_hash_table_insert(server->context.table, opt->resources[i].path, (http_hash_table_value_t) opt->resources[i].https_callback);
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
    if(opt->cert == NULL) {
      if(SSL_CTX_use_certificate_file(server->tls.ctx, opt->cert_path, opt->cert_type) != 1) {
        goto err_ctx;
      }
    } else if(SSL_CTX_use_certificate(server->tls.ctx, opt->cert) != 1) {
      goto err_ctx;
    }
    if(opt->key == NULL) {
      if(SSL_CTX_use_PrivateKey_file(server->tls.ctx, opt->key_path, opt->key_type) != 1) {
        goto err_ctx;
      }
    } else if(SSL_CTX_use_PrivateKey(server->tls.ctx, opt->key) != 1) {
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
  return 0;
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
      if(server->context.force_close) {
        tcp_socket_force_close(&socket->tcp);
      } else {
        tcp_socket_stop_receiving_data(&socket->tcp);
        tcp_socket_close(&socket->tcp);
      }
      return;
    }
    break;
  }
  http1_create_message(msg, message);
  (void) tcp_send(&socket->tcp, msg, len);
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
        if(server->context.force_close) {
          tcp_socket_force_close(&socket->tcp);
        } else {
          tcp_socket_stop_receiving_data(&socket->tcp);
          tcp_socket_close(&socket->tcp);
        }
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
        if(server->context.force_close) {
          tcp_socket_force_close(&socket->tcp);
        } else {
          tcp_socket_stop_receiving_data(&socket->tcp);
          tcp_socket_close(&socket->tcp);
        }
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
  struct http_header req_headers[255];
  struct http_message request = {0};
  request.headers = req_headers;
  request.headers_len = 255;
  int state;
  socket->context.read_buffer[socket->context.read_used] = 0;
  while(1) {
    state = http1_parse_request(socket->context.read_buffer, socket->context.read_used, &socket->context.session, server->context.request_settings, &request);
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
    const struct http_header* const connection = http1_seek_header(&request, "connection", 10);
    int close_connection = 0;
    if(connection != NULL && connection->value_len == 5 && strncasecmp(connection->value, "close", 5) == 0) {
      close_connection = 1;
    }
    struct http_header res_headers[255];
    struct http_message response = {0};
    response.headers = res_headers;
    switch(state) {
      case http_valid: {
        const char save = request.path[request.path_len];
        request.path[request.path_len] = 0;
        http_hash_table_value_t func = http_hash_table_find(server->context.table, request.path);
        request.path[request.path_len] = save;
        if(func == NULL) {
          response.status_code = http_s_not_found;
          response.reason_phrase = http1_default_reason_phrase(response.status_code);
        } else {
          ((void (*)(struct http_server*, struct http_serversock*, struct http_message*, struct http_message*)) func)(server, socket, &request, &response);
        }
        break;
      }
      case http_body_too_long: {
        /* If Expect header is after Content-Length, we will never know about it.
        Well... non-compliant, but that's a little bit dumb. Let's just send
        http_s_payload_too_large. It means pretty much the same thing. */
        response.status_code = http_s_payload_too_large;
        response.reason_phrase = http1_default_reason_phrase(response.status_code);
        break;
      }
      case http_path_too_long: {
        response.status_code = http_s_request_uri_too_long;
        response.reason_phrase = http1_default_reason_phrase(response.status_code);
        break;
      }
      case http_invalid_version: {
        response.status_code = http_s_http_version_not_supported;
        response.reason_phrase = http1_default_reason_phrase(response.status_code);
        break;
      }
      case http_header_name_too_long: {
        response.status_code = http_s_request_header_fields_too_large;
        response.reason_phrase = "Request Header Name Too Large";
        break;
      }
      case http_header_value_too_long: {
        response.status_code = http_s_request_header_fields_too_large;
        response.reason_phrase = "Request Header Value Too Large";
        break;
      }
      case http_too_many_headers: {
        response.status_code = http_s_request_header_fields_too_large;
        response.reason_phrase = "Request Has Too Many Headers";
        break;
      }
      case http_method_too_long: {
        response.status_code = http_s_bad_request;
        response.reason_phrase = "Request Method Too Large";
        break;
      }
      case http_transfer_not_supported: {
        response.status_code = http_s_not_implemented;
        response.reason_phrase = "Transfer-Encoding Not Implemented";
        break;
      }
      case http_encoding_not_supported: {
        response.status_code = http_s_not_implemented;
        response.reason_phrase = "Content-Encoding Not Implemented";
        break;
      }
      case http_corrupted_body_compression: {
        response.status_code = http_s_bad_request;
        response.reason_phrase = "Malformed Body Compression";
        break;
      }
      default: {
        response.status_code = http_s_bad_request;
        response.reason_phrase = http1_default_reason_phrase(response.status_code);
        break;
      }
    }
    if(response.status_code != 0) {
      /* We have a response to send. Automatically attach some headers. */
#define add_header(a,b,c,d) response.headers[response.headers_len++]=(struct http_header){a,c,b,d}
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
              body = gzip_compress2(response.body, response.body_len, &body_len,
                pick(server->context.quality, Z_DEFAULT_COMPRESSION),
                pick(server->context.window_bits, Z_GZIP_DEFAULT_WINDOW_BITS),
                pick(server->context.mem_level, Z_DEFAULT_MEM_LEVEL),
                pick(server->context.mode, Z_DEFAULT_STRATEGY));
              break;
            }
            case http_e_deflate: {
              body = deflate_compress2(response.body, response.body_len, &body_len,
                pick(server->context.quality, Z_DEFAULT_COMPRESSION),
                pick(server->context.window_bits, Z_DEFLATE_DEFAULT_WINDOW_BITS),
                pick(server->context.mem_level, Z_DEFAULT_MEM_LEVEL),
                pick(server->context.mode, Z_DEFAULT_STRATEGY));
              break;
            }
            case http_e_brotli: {
              body = brotli_compress2(response.body, response.body_len, &body_len,
                pick(server->context.quality, BROTLI_DEFAULT_QUALITY),
                pick(server->context.window_bits, BROTLI_DEFAULT_WINDOW),
                pick(server->context.mode, BROTLI_MODE_GENERIC));
              break;
            }
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
        }
      }
      out:
      server->context.compression_is_required = 0;
      server->context.quality = 0;
      server->context.window_bits = 0;
      server->context.mem_level = 0;
      server->context.mode = 0;
      
      if(response.body_len != 0) {
        char content_length[11];
        const int len = sprintf(content_length, "%u", response.body_len);
        if(len < 0) {
          goto err_res;
        }
        add_header("Content-Length", 14, content_length, len);
      }
      
      char date_header[30];
      date_header[0] = 0;
      struct tm tms;
      if(gmtime_r(&(time_t){time(NULL)}, &tms) != NULL && strftime(date_header, 30, "%a, %d %b %Y %H:%M:%S GMT", &tms) != 0) {
        add_header("Date", 4, date_header, 29);
      }
      add_header("Server", 6, "shnet", 5);
#undef add_header
      http_serversock_send(socket, &response);
    }
    if(close_connection == 0) {
      return;
    }
    
    err_res:
    if(response.allocated_body) {
      free(response.body);
    }
    goto err;
  }
  return;
  
  err:
  socket->context.expected = 1;
  (void) time_manager_cancel_timeout(server->context.manager, &socket->context.timeout);
  free(socket->context.read_buffer);
  socket->context.read_buffer = NULL;
  if(server->context.force_close) {
    tcp_socket_force_close(&socket->tcp);
  } else {
    tcp_socket_stop_receiving_data(&socket->tcp);
    tcp_socket_close(&socket->tcp);
  }
}

static int http_serversock_onnomem(struct tcp_socket* soc) {
  return server->callbacks->onnomem(server);
}

static void http_serversock_onclose(struct tcp_socket* soc) {
  if(!socket->context.expected) {
    (void) time_manager_cancel_timeout(server->context.manager, &socket->context.timeout);
  }
  tcp_socket_free(&socket->tcp);
}

static void http_serversock_onfree(struct tcp_socket* soc) {
  if(socket->context.read_buffer != NULL) {
    free(socket->context.read_buffer);
  }
  (void) memset(socket + 1, 0, sizeof(struct http_serversock) - sizeof(struct tcp_socket));
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
      if(server->context.force_close) {
        tcp_socket_force_close(&socket->tls.tcp);
      } else {
        tls_socket_stop_receiving_data(&socket->tls);
        tls_socket_close(&socket->tls);
      }
      return;
    }
    break;
  }
  http1_create_message(msg, message);
  (void) tls_send(&socket->tls, msg, len);
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
        if(server->context.force_close) {
          tcp_socket_force_close(&socket->tls.tcp);
        } else {
          tls_socket_stop_receiving_data(&socket->tls);
          tls_socket_close(&socket->tls);
        }
        return;
      }
      break;
    }
  }
}

static void https_serversock_onmessage(struct tls_socket* soc) {
  struct http_header req_headers[255];
  struct http_message request = {0};
  request.headers = req_headers;
  request.headers_len = 255;
  int state;
  while(1) {
    state = http1_parse_request(socket->tls.read_buffer, socket->tls.read_used, &socket->context.session, server->context.request_settings, &request);
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
    const struct http_header* const connection = http1_seek_header(&request, "connection", 10);
    int close_connection = 0;
    if(connection != NULL && connection->value_len == 5 && strncasecmp(connection->value, "close", 5) == 0) {
      close_connection = 1;
    }
    struct http_header res_headers[255];
    struct http_message response = {0};
    response.headers = res_headers;
    switch(state) {
      case http_valid: {
        const char save = request.path[request.path_len];
        request.path[request.path_len] = 0;
        http_hash_table_value_t func = http_hash_table_find(server->context.table, request.path);
        request.path[request.path_len] = save;
        if(func == NULL) {
          response.status_code = http_s_not_found;
          response.reason_phrase = http1_default_reason_phrase(response.status_code);
        } else {
          ((void (*)(struct https_server*, struct https_serversock*, struct http_message*, struct http_message*)) func)(server, socket, &request, &response);
        }
        break;
      }
      case http_body_too_long: {
        response.status_code = http_s_payload_too_large;
        response.reason_phrase = http1_default_reason_phrase(response.status_code);
        break;
      }
      case http_path_too_long: {
        response.status_code = http_s_request_uri_too_long;
        response.reason_phrase = http1_default_reason_phrase(response.status_code);
        break;
      }
      case http_invalid_version: {
        response.status_code = http_s_http_version_not_supported;
        response.reason_phrase = http1_default_reason_phrase(response.status_code);
        break;
      }
      case http_header_name_too_long: {
        response.status_code = http_s_request_header_fields_too_large;
        response.reason_phrase = "Request Header Name Too Large";
        break;
      }
      case http_header_value_too_long: {
        response.status_code = http_s_request_header_fields_too_large;
        response.reason_phrase = "Request Header Value Too Large";
        break;
      }
      case http_too_many_headers: {
        response.status_code = http_s_request_header_fields_too_large;
        response.reason_phrase = "Request Has Too Many Headers";
        break;
      }
      case http_method_too_long: {
        response.status_code = http_s_bad_request;
        response.reason_phrase = "Request Method Too Large";
        break;
      }
      case http_transfer_not_supported: {
        response.status_code = http_s_not_implemented;
        response.reason_phrase = "Transfer-Encoding Not Implemented";
        break;
      }
      case http_encoding_not_supported: {
        response.status_code = http_s_not_implemented;
        response.reason_phrase = "Content-Encoding Not Implemented";
        break;
      }
      case http_corrupted_body_compression: {
        response.status_code = http_s_bad_request;
        response.reason_phrase = "Malformed Body Compression";
        break;
      }
      default: {
        response.status_code = http_s_bad_request;
        response.reason_phrase = http1_default_reason_phrase(response.status_code);
        break;
      }
    }
    if(response.status_code != 0) {
#define add_header(a,b,c,d) response.headers[response.headers_len++]=(struct http_header){a,c,b,d}
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
              body = gzip_compress2(response.body, response.body_len, &body_len,
                pick(server->context.quality, Z_DEFAULT_COMPRESSION),
                pick(server->context.window_bits, Z_GZIP_DEFAULT_WINDOW_BITS),
                pick(server->context.mem_level, Z_DEFAULT_MEM_LEVEL),
                pick(server->context.mode, Z_DEFAULT_STRATEGY));
              break;
            }
            case http_e_deflate: {
              body = deflate_compress2(response.body, response.body_len, &body_len,
                pick(server->context.quality, Z_DEFAULT_COMPRESSION),
                pick(server->context.window_bits, Z_DEFLATE_DEFAULT_WINDOW_BITS),
                pick(server->context.mem_level, Z_DEFAULT_MEM_LEVEL),
                pick(server->context.mode, Z_DEFAULT_STRATEGY));
              break;
            }
            case http_e_brotli: {
              body = brotli_compress2(response.body, response.body_len, &body_len,
                pick(server->context.quality, BROTLI_DEFAULT_QUALITY),
                pick(server->context.window_bits, BROTLI_DEFAULT_WINDOW),
                pick(server->context.mode, BROTLI_MODE_GENERIC));
              break;
            }
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
        }
      }
      out:
      server->context.compression_is_required = 0;
      server->context.quality = 0;
      server->context.window_bits = 0;
      server->context.mem_level = 0;
      server->context.mode = 0;
      
      if(response.body_len != 0) {
        char content_length[11];
        const int len = sprintf(content_length, "%u", response.body_len);
        if(len < 0) {
          goto err_res;
        }
        add_header("Content-Length", 14, content_length, len);
      }
      
      char date_header[30];
      date_header[0] = 0;
      struct tm tms;
      if(gmtime_r(&(time_t){time(NULL)}, &tms) != NULL && strftime(date_header, 30, "%a, %d %b %Y %H:%M:%S GMT", &tms) != 0) {
        add_header("Date", 4, date_header, 29);
      }
      add_header("Server", 6, "shnet", 5);
#undef add_header
      https_serversock_send(socket, &response);
    }
    if(close_connection == 0) {
      return;
    }
    
    err_res:
    if(response.allocated_body) {
      free(response.body);
    }
    goto err;
  }
  return;
  
  err:
  socket->context.expected = 1;
  (void) time_manager_cancel_timeout(server->context.manager, &socket->context.timeout);
  free(socket->tls.read_buffer);
  socket->tls.read_buffer = NULL;
  if(server->context.force_close) {
    tcp_socket_force_close(&socket->tls.tcp);
  } else {
    tls_socket_stop_receiving_data(&socket->tls);
    tls_socket_close(&socket->tls);
  }
}

static int https_serversock_onnomem(struct tls_socket* soc) {
  return server->callbacks->onnomem(server);
}

static void https_serversock_onclose(struct tls_socket* soc) {
  if(!socket->context.expected) {
    (void) time_manager_cancel_timeout(server->context.manager, &socket->context.timeout);
  }
  tls_socket_free(&socket->tls);
}

static void https_serversock_onfree(struct tls_socket* soc) {
  if(socket->context.read_buffer != NULL) {
    free(socket->context.read_buffer);
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