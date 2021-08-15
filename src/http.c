#include "http.h"
#include "debug.h"
#include "base64.h"

#include <errno.h>
#include <string.h>
#include <inttypes.h>

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


static int  http_server_onconnection(struct tcp_socket*, const struct sockaddr*);

static int  http_server_onnomem(struct tcp_server*);

static void http_server_onerror(struct tcp_server*);

static void http_server_onshutdown(struct tcp_server*);


static int  https_server_onconnection(struct tls_socket*, const struct sockaddr*);

static int  https_server_onnomem(struct tls_server*);

static void https_server_onerror(struct tls_server*);

static void https_server_onshutdown(struct tls_server*);


static struct tcp_socket_callbacks http_client_callbacks = {
  NULL,
  http_client_onopen,
  http_client_onmessage,
  NULL,
  NULL,
  http_client_onnomem,
  http_client_onclose,
  http_client_onfree
};

static struct tcp_socket_settings  http_client_settings = { 1, 1, 0, 0, 0, 0, 1, 0 };

static struct tcp_socket_callbacks http_serversock_callbacks = {
  NULL,
  http_serversock_onopen,
  http_serversock_onmessage,
  NULL,
  NULL,
  http_serversock_onnomem,
  http_serversock_onclose,
  http_serversock_onfree
};

static struct tcp_socket_settings  http_serversock_settings = { 1, 0, 0, 0, 0, 0, 1, 0 };


static struct tls_socket_callbacks https_client_callbacks = {
  NULL,
  https_client_onopen,
  https_client_onmessage,
  https_client_onnomem,
  https_client_onclose,
  https_client_onfree
};

static struct tcp_socket_settings  https_client_settings = { 1, 1, 0, 0, 0, 0, 1, 1 };

static struct tls_socket_callbacks https_serversock_callbacks = {
  NULL,
  https_serversock_onopen,
  https_serversock_onmessage,
  https_serversock_onnomem,
  https_serversock_onclose,
  https_serversock_onfree
};

static struct tcp_socket_settings  https_serversock_settings = { 1, 0, 0, 0, 0, 0, 1, 1 };


static struct tcp_server_callbacks http_server_callbacks = {
  http_server_onconnection,
  http_server_onnomem,
  http_server_onerror,
  http_server_onshutdown
};

static struct tls_server_callbacks https_server_callbacks = {
  https_server_onconnection,
  https_server_onnomem,
  https_server_onerror,
  https_server_onshutdown
};


static struct http_parser_settings http_request_parser_settings = {
  .max_method_len = 16,
  .max_path_len = 255,
  .max_headers = 32,
  .max_header_name_len = 32,
  .max_header_value_len = 1024,
  .max_query_len = 64,
  .max_body_len = 1024000 /* ~1MB */
};

static struct http_parser_settings http_prerequest_parser_settings = {
  .max_method_len = 255,
  .max_path_len = 255,
  .max_query_len = 255,
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


const char* http_str_close_reason(const int err) {
  switch(err) {
    case http_unreachable: return "http_unreachable";
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
    uri.port_ptr = port + 1;
    uri.port = atoi(port + 1);
    uri.hostname_len = (uintptr_t) port - (uintptr_t) str - 7 - uri.secure;
    uri.path = memchr(port, '/', len - ((uintptr_t) port - (uintptr_t) str));
    if(uri.path == NULL) {
      uri.path = "/";
      uri.path_len = 1;
      uri.port_len = len - 1 - ((uintptr_t) port - (uintptr_t) str);
    } else {
      uri.path_len = len - ((uintptr_t) uri.path - (uintptr_t) str);
      uri.port_len = (uintptr_t) uri.path - (uintptr_t) port - 1;
    }
  } else {
    uri.path = memchr(uri.hostname, '/', len - 7 - uri.secure);
    if(uri.path == NULL) {
      uri.path = "/";
      uri.path_len = 1;
      uri.hostname_len = len - 7 - uri.secure;
    } else {
      uri.path_len = len - ((uintptr_t) uri.path - (uintptr_t) str);
      uri.hostname_len = (uintptr_t) uri.path - (uintptr_t) uri.hostname;
    }
  }
  return uri;
}

static struct http_requests* http_setup_requests(struct http_uri* const uri, struct http_options* const opt) {
  struct http_request req = {0};
  if(opt->requests_len == 0) {
    /* No requests means a GET request by default */
    opt->requests_len = 1;
    opt->requests = &req;
  }
  struct http_requests* requests = malloc(sizeof(struct http_requests) * opt->requests_len);
  if(requests == NULL) {
    return NULL;
  }
  for(uint32_t k = 0; k < opt->requests_len; ++k) {
    struct http_header headers[255];
    uint8_t idx = 0;
    uint8_t done_idx = 0;
#define set_header(a,b,c,d) \
do { \
  int done = 0; \
  if(opt->requests[k].request != NULL && opt->requests[k].request->headers != NULL) { \
    for(uint8_t i = done_idx; i < opt->requests[k].request->headers_len; ++i) { \
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
          headers[idx++] = (struct http_header){a,c,d,b,0,0}; \
        } \
        break; \
      } \
    } \
  } \
  if(!done) { \
    headers[idx++] = (struct http_header){a,c,d,b,0,0}; \
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
              deflateEnd(stream);
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
              deflateEnd(stream);
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
          if(opt->requests[k].compression_isnt_required) {
            goto out;
          } else {
            goto err;
          }
        }
        void* const ptr = realloc(body, body_len);
        if(opt->requests[k].request->alloc_body) {
          free(opt->requests[k].request->body);
        }
        if(ptr != NULL) {
          opt->requests[k].request->body = ptr;
          opt->requests[k].request->body_len = body_len;
        } else {
          opt->requests[k].request->body = body;
        }
        opt->requests[k].request->alloc_body = 1;
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
    char content_length[21];
    const int len = sprintf(content_length, "%" PRIu64, request.body_len);
    if(len < 0) {
      goto err;
    }
    set_header("Content-Length", 14, content_length, len);
    set_header("Host", 4, uri->hostname, uri->hostname_len + (uri->port_len ? uri->port_len + 1 : 0));
    if(opt->requests[k].no_cache) {
      set_header("Pragma", 6, "no-cache", 8);
    }
    set_header("User-Agent", 10, "Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/92.0.4515.115 Safari/537.36", 105);
#undef set_header
    request.headers_len = idx;
    requests[k].len = http1_message_length(&request);
    requests[k].msg = malloc(requests[k].len);
    if(requests[k].msg == NULL) {
      goto err;
    }
    http1_create_message(requests[k].msg, &request);
    if(opt->requests[k].request != NULL && opt->requests[k].request->alloc_body) {
      free(opt->requests[k].request->body);
    }
    if(opt->requests[k].response_settings != NULL) {
      requests[k].response_settings = opt->requests[k].response_settings;
    } else {
      requests[k].response_settings = &http_response_parser_settings;
    }
    continue;
    
    err:
    if(opt->requests[k].request != NULL && opt->requests[k].request->alloc_body) {
      free(opt->requests[k].request->body);
    }
    for(uint8_t i = 0; i < k; ++i) {
      free(requests[i].msg);
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

static int http_socket(struct http_uri* const uri, struct http_options* const opt, struct http_requests* requests) {
  struct http_socket* const socket = calloc(1, sizeof(struct http_socket));
  if(socket == NULL) {
    return -1;
  }
  socket->tcp.settings = http_client_settings;
  socket->tcp.callbacks = &http_client_callbacks;
  socket->tcp.info = opt->info;
  socket->tcp.epoll = opt->epoll;
  socket->context.timeout_after = opt->timeout_after ? opt->timeout_after : 10;
  socket->context.read_growth = opt->read_growth ? opt->read_growth : 16384;
  socket->context.requests = requests;
  socket->context.requests_size = opt->requests_len;
  socket->callbacks = opt->http_callbacks;
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
      socket->context.alloc_manager = 1;
    }
  }
  char hostname[256];
  (void) memcpy(hostname, uri->hostname, uri->hostname_len);
  hostname[uri->hostname_len] = 0;
  char port[6];
  if(uri->port_ptr != NULL) {
    (void) memcpy(port, uri->port_ptr, uri->port_len);
    port[uri->port_len] = 0;
  } else {
    (void) memcpy(port, "80", 3);
  }
  struct tcp_socket_options options = {
    hostname,
    port,
    opt->family,
    opt->flags
  };
  if(tcp_socket(&socket->tcp, &options) != 0) {
    goto err_time;
  }
  
  return 0;
  
  err_time:
  if(socket->context.alloc_manager) {
    time_manager_stop(socket->context.manager);
    time_manager_free(socket->context.manager);
    free(socket->context.manager);
  }
  err_sock:
  free(socket);
  return -1;
}

#define socket ((struct http_socket*) soc)

static void http_stop_socket(void* soc) {
  socket->callbacks->onclose(socket, http_timeouted);
  socket->context.expected = 1;
  tcp_socket_force_close(&socket->tcp);
}

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
  (void) tcp_send(&socket->tcp, socket->context.requests[socket->context.requests_used].msg, socket->context.requests[socket->context.requests_used].len, tcp_read_only | tcp_dont_free);
}

static void http_client_onmessage(struct tcp_socket* soc) {
  if(socket->context.expected) {
    return;
  }
  void* const old_ptr = socket->context.read_buffer;
  while(1) {
    if(socket->context.read_used + socket->context.read_growth > socket->context.read_size) {
      while(1) {
        char* const ptr = realloc(socket->context.read_buffer, socket->context.read_used + socket->context.read_growth);
        if(ptr != NULL) {
          socket->context.read_buffer = ptr;
          socket->context.read_size = socket->context.read_used + socket->context.read_growth;
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
  if(socket->context.read_used == 0) {
    return;
  }
  socket->context.response.headers = socket->context.headers;
  socket->context.response.headers_len = 255;
  if(socket->context.read_buffer != old_ptr) {
    http1_convert_message(old_ptr, socket->context.read_buffer, &socket->context.response);
  }
  int state;
  while(1) {
    state = http1_parse_response(socket->context.read_buffer, socket->context.read_used, &socket->context.session, socket->context.requests[socket->context.requests_used].response_settings, &socket->context.response);
    if(state == http_out_of_memory) {
      if(socket->callbacks->onnomem(socket) == 0) {
        continue;
      }
      (void) time_manager_cancel_timeout(socket->context.manager, &socket->context.timeout);
      socket->callbacks->onclose(socket, http_no_memory);
      socket->context.expected = 1;
      goto err;
    }
    break;
  }
  if(state != http_incomplete) {
    (void) time_manager_cancel_timeout(socket->context.manager, &socket->context.timeout);
    socket->context.session = (struct http_parser_session){0};
    if(state == http_valid) {
      socket->callbacks->onresponse(socket, &socket->context.response);
    } else {
      socket->callbacks->onclose(socket, http_invalid_response);
      socket->context.expected = 1;
      goto err;
    }
    if(++socket->context.requests_used == socket->context.requests_size) {
      socket->context.expected = 1;
      goto err;
    } else {
      const struct http_header* const connection = http1_seek_header(&socket->context.response, "connection", 10);
      const int conn_is_close = connection != NULL && connection->value_len == 5 && strncasecmp(connection->value, "close", 5) == 0;
      if(socket->context.response.alloc_body) {
        free(socket->context.response.body);
      }
      free(socket->context.read_buffer);
      socket->context.read_buffer = NULL;
      socket->context.read_used = 0;
      socket->context.read_size = 0;
      socket->context.response = (struct http_message){0};
      if(conn_is_close) {
        socket->callbacks->onclose(socket, http_no_keepalive);
        socket->context.expected = 1;
        goto err;
      } else {
        http_client_onopen(&socket->tcp);
      }
    }
  }
  return;
  
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
  if(socket->context.read_buffer != NULL) {
    free(socket->context.read_buffer);
  }
  if(socket->context.response.alloc_body) {
    free(socket->context.response.body);
  }
  if(socket->context.alloc_manager) {
    time_manager_stop(socket->context.manager);
    time_manager_free(socket->context.manager);
    free(socket->context.manager);
  }
  http_free_requests(socket->context.requests, socket->context.requests_size);
}

#undef socket

static int https_socket(struct http_uri* const uri, struct http_options* const opt, struct http_requests* requests) {
  struct https_socket* const socket = calloc(1, sizeof(struct https_socket));
  if(socket == NULL) {
    return -1;
  }
  socket->tls.tcp.settings = https_client_settings;
  socket->tls.tcp.info = opt->info;
  socket->tls.tcp.epoll = opt->epoll;
  socket->tls.callbacks = &https_client_callbacks;
  socket->tls.ctx = opt->ctx;
  socket->tls.read_growth = opt->read_growth ? opt->read_growth : 16384;
  socket->context.timeout_after = opt->timeout_after ? opt->timeout_after : 10;
  socket->context.requests = requests;
  socket->context.requests_size = opt->requests_len;
  socket->callbacks = opt->https_callbacks;
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
      socket->context.alloc_manager = 1;
    }
  }
  char hostname[256];
  (void) memcpy(hostname, uri->hostname, uri->hostname_len);
  hostname[uri->hostname_len] = 0;
  char port[6];
  if(uri->port_ptr != NULL) {
    (void) memcpy(port, uri->port_ptr, uri->port_len);
    port[uri->port_len] = 0;
  } else {
    (void) memcpy(port, "443", 4);
  }
  struct tls_socket_options options = {
    .tcp = {
      hostname,
      port,
      opt->family,
      opt->flags
    }
  };
  if(tls_socket(&socket->tls, &options) != 0) {
    goto err_time;
  }
  
  return 0;
  
  err_time:
  if(socket->context.alloc_manager) {
    time_manager_stop(socket->context.manager);
    time_manager_free(socket->context.manager);
    free(socket->context.manager);
  }
  err_sock:
  free(socket);
  return -1;
}

#define socket ((struct https_socket*) soc)

static void https_stop_socket(void* soc) {
  socket->callbacks->onclose(socket, http_timeouted);
  socket->context.expected = 1;
  tcp_socket_force_close(&socket->tls.tcp);
}

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
  (void) tls_send(&socket->tls, socket->context.requests[socket->context.requests_used].msg, socket->context.requests[socket->context.requests_used].len, tls_read_only | tls_dont_free);
}

static void https_client_onmessage(struct tls_socket* soc) {
  if(socket->context.expected) {
    return;
  }
  socket->context.response.headers = socket->context.headers;
  socket->context.response.headers_len = 255;
  if(socket->context.read_buffer == NULL) {
    socket->context.read_buffer = socket->tls.read_buffer;
  } else if(socket->context.read_buffer != socket->tls.read_buffer) {
    http1_convert_message(socket->context.read_buffer, socket->tls.read_buffer, &socket->context.response);
    socket->context.read_buffer = socket->tls.read_buffer;
  }
  int state;
  while(1) {
    state = http1_parse_response(socket->tls.read_buffer, socket->tls.read_used, &socket->context.session, socket->context.requests[socket->context.requests_used].response_settings, &socket->context.response);
    if(state == http_out_of_memory) {
      if(socket->callbacks->onnomem(socket) == 0) {
        continue;
      }
      (void) time_manager_cancel_timeout(socket->context.manager, &socket->context.timeout);
      socket->callbacks->onclose(socket, http_no_memory);
      socket->context.expected = 1;
      goto err;
    }
    break;
  }
  if(state != http_incomplete) {
    (void) time_manager_cancel_timeout(socket->context.manager, &socket->context.timeout);
    socket->context.session = (struct http_parser_session){0};
    if(state == http_valid) {
      socket->callbacks->onresponse(socket, &socket->context.response);
    } else {
      socket->callbacks->onclose(socket, http_invalid_response);
      socket->context.expected = 1;
      goto err;
    }
    if(++socket->context.requests_used == socket->context.requests_size) {
      socket->context.expected = 1;
      goto err;
    } else {
      const struct http_header* const connection = http1_seek_header(&socket->context.response, "connection", 10);
      const int conn_is_close = connection != NULL && connection->value_len == 5 && strncasecmp(connection->value, "close", 5) == 0;
      if(socket->context.response.alloc_body) {
        free(socket->context.response.body);
      }
      free(socket->tls.read_buffer);
      socket->tls.read_buffer = NULL;
      socket->tls.read_used = 0;
      socket->tls.read_size = 0;
      socket->context.response = (struct http_message){0};
      if(conn_is_close) {
        socket->callbacks->onclose(socket, http_no_keepalive);
        socket->context.expected = 1;
        goto err;
      } else {
        https_client_onopen(&socket->tls);
      }
    }
  }
  return;
  
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
  if(socket->context.alloc_manager) {
    time_manager_stop(socket->context.manager);
    time_manager_free(socket->context.manager);
    free(socket->context.manager);
  }
  if(socket->context.response.alloc_body) {
    free(socket->context.response.body);
  }
  http_free_requests(socket->context.requests, socket->context.requests_size);
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
    if(https_socket(&uri, options, requests) != 0) {
      goto err;
    }
  } else {
    if(http_socket(&uri, options, requests) != 0) {
      goto err;
    }
  }
  return 0;
  
  err:
  http_free_requests(requests, options->requests_len);
  return -1;
}



static int http_setup_server(struct http_uri* const uri, struct http_server_options* const opt) {
  struct http_server* const server = calloc(1, sizeof(struct http_server));
  if(server == NULL) {
    return -1;
  }
  server->tcp.settings = opt->tcp_settings;
  server->tcp.callbacks = &http_server_callbacks;
  server->tcp.socket_size = opt->socket_size ? opt->socket_size : sizeof(struct http_serversock);
  server->context.default_settings = opt->default_settings ? opt->default_settings : &http_request_parser_settings;
  server->context.read_growth = opt->read_growth ? opt->read_growth : 16384;
  server->context.timeout_after = opt->timeout_after ? opt->timeout_after : 10;
  server->callbacks = opt->http_callbacks;
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
    server->context.alloc_table = 1;
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
      server->context.alloc_manager = 1;
    }
  }
  char hostname[256];
  (void) memcpy(hostname, uri->hostname, uri->hostname_len);
  hostname[uri->hostname_len] = 0;
  char port[6];
  if(uri->port_ptr != NULL) {
    (void) memcpy(port, uri->port_ptr, uri->port_len);
    port[uri->port_len] = 0;
  } else {
    (void) memcpy(port, "80", 3);
  }
  struct tcp_server_options options = {
    opt->info,
    hostname,
    port,
    opt->family,
    opt->flags
  };
  if(tcp_server(&server->tcp, &options) != 0) {
    goto err_time;
  }
  opt->http_server = server;
  return 0;
  
  err_time:
  if(server->context.alloc_manager) {
    time_manager_stop(server->context.manager);
    time_manager_free(server->context.manager);
    free(server->context.manager);
  }
  err_table:
  if(server->context.alloc_table) {
    http_hash_table_free(server->context.table);
    free(server->context.table);
  }
  err_serv:
  free(server);
  return -1;
}

void http_server_foreach_conn(struct http_server* const server, void (*callback)(struct http_serversock*, void*), void* data) {
  tcp_server_foreach_conn(&server->tcp, (void (*)(struct tcp_socket*,void*)) callback, data);
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

#define http_server ((struct http_server*) socket->server)
#define https_server ((struct https_server*) socket->server)

static void http_serversock_timeout(struct tcp_socket* const socket) {
  struct http_header headers[3];
  struct http_message response = {0};
  response.headers = headers;
  response.status_code = http_s_request_timeout;
  response.reason_phrase = http1_default_reason_phrase(response.status_code);
  response.reason_phrase_len = http1_default_reason_phrase_len(response.status_code);
  
  char date_header[30];
  date_header[0] = 0;
  struct tm tms;
  if(gmtime_r(&(time_t){time(NULL)}, &tms) != NULL) {
    const size_t len = strftime(date_header, 30, "%a, %d %b %Y %H:%M:%S GMT", &tms);
    if(len != 0) {
      headers[0] = (struct http_header) { "Connection", "close", 5, 10, 0, 0 };
      headers[1] = (struct http_header) { "Date", date_header, len, 4, 0, 0 };
      headers[2] = (struct http_header) { "Server", "shnet", 5, 6, 0, 0 };
      response.headers_len = 3;
    } else {
      headers[0] = (struct http_header) { "Connection", "close", 5, 10, 0, 0 };
      headers[1] = (struct http_header) { "Server", "shnet", 5, 6, 0, 0 };
      response.headers_len = 2;
    }
  } else {
    headers[0] = (struct http_header) { "Connection", "close", 5, 10, 0, 0 };
    headers[1] = (struct http_header) { "Server", "shnet", 5, 6, 0, 0 };
    response.headers_len = 2;
  }
  char* msg;
  const uint64_t len = http1_message_length(&response);
  while(1) {
    msg = malloc(len);
    if(msg == NULL) {
      if(socket->net.secure) {
        if(https_server->callbacks->onnomem(https_server) == 0) {
          continue;
        }
      } else {
        if(http_server->callbacks->onnomem(http_server) == 0) {
          continue;
        }
      }
      return;
    }
    break;
  }
  http1_create_message(msg, &response);
  if(socket->net.secure) {
    (void) tls_send((struct tls_socket*) socket, msg, len, tls_read_only);
  } else {
    (void) tcp_send(socket, msg, len, tcp_read_only);
  }
}

#undef https_server
#undef http_server

#define socket ((struct http_serversock*) data)
#define server ((struct http_server*) socket->tcp.server)

static void http_serversock_stop_socket(void* data) {
  socket->context.expected = 1;
  tcp_socket_dont_receive_data(&socket->tcp);
  http_serversock_timeout(&socket->tcp);
  tcp_socket_close(&socket->tcp);
}

#undef server
#undef socket

static void http_server_free(struct http_server* const server) {
  if(server->context.alloc_manager) {
    time_manager_stop(server->context.manager);
    time_manager_free(server->context.manager);
    free(server->context.manager);
  }
  if(server->context.alloc_table) {
    http_hash_table_free(server->context.table);
    free(server->context.table);
  }
}

#define server ((struct http_server*) serv)

static int http_server_onnomem(struct tcp_server* serv) {
  return server->callbacks->onnomem(server);
}

static void http_server_onerror(struct tcp_server* serv) {
  if(server->callbacks->onerror != NULL) {
    server->callbacks->onerror(server);
  }
}

static void http_server_onshutdown(struct tcp_server* serv) {
  server->callbacks->onshutdown(server);
  http_server_free(server);
  tcp_server_free(&server->tcp);
  free(server);
}

#undef server

#define socket ((struct http_serversock*) soc)
#define server ((struct http_server*) soc->server)

static int http_server_onconnection(struct tcp_socket* soc, const struct sockaddr* addr) {
  socket->tcp.settings = http_serversock_settings;
  socket->tcp.callbacks = &http_serversock_callbacks;
  return 0;
}

static void http_serversock_onopen(struct tcp_socket* soc) {
  (void) tcp_socket_keepalive(&socket->tcp);
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
  if(socket->context.expected && socket->context.protocol != http_p_websocket) {
    return;
  }
  while(1) {
    if(socket->context.read_used + server->context.read_growth > socket->context.read_size) {
      while(1) {
        char* const ptr = realloc(socket->context.read_buffer, socket->context.read_used + server->context.read_growth);
        if(ptr != NULL) {
          socket->context.read_buffer = ptr;
          socket->context.read_size = socket->context.read_used + server->context.read_growth;
          break;
        }
        if(server->callbacks->onnomem(server) == 0) {
          continue;
        }
        (void) time_manager_cancel_timeout(server->context.manager, &socket->context.timeout);
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
    return;
  }
  if(socket->context.protocol == http_p_websocket) {
    goto websocket;
  }
  socket->context.message.headers = socket->context.headers;
  socket->context.message.headers_len = 255;
  int state;
  if(socket->context.init_parsed == 0) {
    state = http1_parse_request(socket->context.read_buffer, socket->context.read_used, &socket->context.session, &http_prerequest_parser_settings, &socket->context.message);
    switch(state) {
      case http_incomplete: return;
      case http_valid: {
        const char save = socket->context.message.path[socket->context.message.path_len];
        socket->context.message.path[socket->context.message.path_len] = 0;
        socket->context.entry = http_hash_table_find(server->context.table, socket->context.message.path);
        socket->context.message.path[socket->context.message.path_len] = save;
        if(socket->context.entry != NULL) {
          if(socket->context.entry->data != NULL) {
            socket->context.settings = socket->context.entry->data;
          } else {
            socket->context.settings = server->context.default_settings;
          }
        } else {
          /* We can't reject the request here without having the whole message
          in the read buffer, because then we would try to parse the rest of the
          message as a new request, while it was continuation of this one. What
          we can do though is set a flag that will tell the parser to not process
          the message in any way - if body is compressed, don't decompress it. If
          it's chunked, don't merge chunks together. */
          socket->context.backup_settings = *server->context.default_settings;
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
    state = http1_parse_request(socket->context.read_buffer, socket->context.read_used, &socket->context.session, socket->context.settings, &socket->context.message);
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
      connection = http1_seek_header(&socket->context.message, "connection", 10);
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
          ((void (*)(struct http_server*, struct http_serversock*, struct http_message*, struct http_message*)) socket->context.entry->func)(server, socket, &socket->context.message, &response);
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
    if(response.status_code != 0) {
      /* We have a response to send. Automatically attach some headers. */
#define add_header(a,b,c,d) response.headers[response.headers_len++]=(struct http_header){a,c,d,b,0,0}
      if(response.close_conn) {
        close_connection = 1;
        add_header("Connection", 10, "close", 5);
      } else if(connection != NULL) {
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
              if(server->context.compression_isnt_required) {
                goto out;
              }
              goto err_res;
            }
            if(server->callbacks->onnomem(server) == 0) {
              continue;
            }
            if(server->context.compression_isnt_required) {
              goto out;
            }
            goto err_res;
          }
        }
        if(response.alloc_body) {
          free(response.body);
        } else {
          response.alloc_body = 1;
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
      server->context.compression_isnt_required = 0;
      server->context.quality = 0;
      server->context.window_bits = 0;
      server->context.mem_level = 0;
      server->context.mode = 0;
      
      char content_length[21];
      int len = sprintf(content_length, "%" PRIu64, response.body_len);
      if(len < 0) {
        goto err_res;
      }
      add_header("Content-Length", 14, content_length, len);
      
      char date_header[30];
      date_header[0] = 0;
      struct tm tms;
      if(gmtime_r(&(time_t){time(NULL)}, &tms) != NULL) {
        len = strftime(date_header, 30, "%a, %d %b %Y %H:%M:%S GMT", &tms);
        if(len != 0) {
          add_header("Date", 4, date_header, len);
        }
      }
      
      char keepalive[30];
      if(!close_connection) {
        len = sprintf(keepalive, "timeout=%" PRIu64, server->context.timeout_after);
        if(len < 0) {
          goto err_res;
        }
        add_header("Keep-Alive", 10, keepalive, len);
      }
      add_header("Server", 6, "shnet", 5);
#undef add_header
      char* msg;
      const uint64_t length = http1_message_length(&response);
      while(1) {
        msg = malloc(length);
        if(msg == NULL) {
          if(server->callbacks->onnomem(server) == 0) {
            continue;
          }
          goto err_res;
        }
        break;
      }
      http1_create_message(msg, &response);
      (void) tcp_send(&socket->tcp, msg, length, tcp_read_only);
    }
    if(response.alloc_body) {
      free(response.body);
    }
    for(uint32_t i = 0; i < response.headers_len; ++i) {
      if(response.headers[i].alloc_name) {
        free(response.headers[i].name);
      }
      if(response.headers[i].alloc_value) {
        free(response.headers[i].value);
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
    if(response.alloc_body) {
      free(response.body);
    }
    for(uint32_t i = 0; i < response.headers_len; ++i) {
      if(response.headers[i].alloc_name) {
        free(response.headers[i].name);
      }
      if(response.headers[i].alloc_value) {
        free(response.headers[i].value);
      }
    }
    goto err;
  }
  return;
  
  err:
  socket->context.expected = 1;
  (void) time_manager_cancel_timeout(server->context.manager, &socket->context.timeout);
  tcp_socket_force_close(&socket->tcp);
  return;
  
  websocket:
  while(1) {
    while(1) {
      state = websocket_parse(socket->context.read_buffer, socket->context.read_used, &socket->context);
      if(state == http_out_of_memory) {
        if(server->callbacks->onnomem(server) == 0) {
          continue;
        }
        goto errr;
      }
      break;
    }
    if(state != http_incomplete) {
      if(state == http_valid) {
        if(socket->context.closing) {
          goto errr;
        }
        if(socket->context.message.opcode < 8) {
          socket->callbacks->onmessage(socket, socket->context.message.body, socket->context.message.body_len);
        } else if(socket->context.message.opcode == 8) {
          socket->context.closing = 1;
          if(socket->context.close_onreadclose) {
            (void) ws_send(socket, socket->context.message.body, socket->context.message.body_len > 1 ? 2 : 0, websocket_close, ws_dont_free);
            tcp_socket_close(&socket->tcp);
          } else if(socket->callbacks->onreadclose != NULL) {
            socket->callbacks->onreadclose(socket, socket->context.message.close_code);
          }
        } else if(socket->context.message.opcode == 9) {
          (void) ws_send(socket, NULL, 0, websocket_pong, ws_dont_free);
        }
        if(socket->context.message.alloc_body) {
          free(socket->context.message.body);
        }
        socket->context.read_used -= socket->context.session.chunk_idx + socket->context.session.last_idx;
        if(socket->context.read_used != 0) {
          (void) memmove(socket->context.read_buffer, socket->context.read_buffer + socket->context.session.chunk_idx + socket->context.session.last_idx, socket->context.read_used);
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
        socket->context.session = (struct http_parser_session){0};
        socket->context.message = (struct http_message){0};
      } else {
        errr:
        socket->context.expected = 1;
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
  if(!socket->context.expected) {
    (void) time_manager_cancel_timeout(server->context.manager, &socket->context.timeout);
  }
  if(socket->context.protocol == http_p_websocket) {
    socket->callbacks->onclose(socket, socket->context.message.close_code);
  }
  tcp_socket_free(&socket->tcp);
}

static void http_serversock_onfree(struct tcp_socket* soc) {
  if(socket->context.read_buffer != NULL) {
    free(socket->context.read_buffer);
  }
  if(socket->context.deflater != NULL) {
    deflater_free(socket->context.deflater);
  }
  if(socket->context.deflater_mutex != NULL) {
    (void) pthread_mutex_destroy(socket->context.deflater_mutex);
    free(socket->context.deflater_mutex);
  }
  if(socket->context.inflater != NULL) {
    inflater_free(socket->context.inflater);
  }
}

#undef server
#undef socket



static int https_setup_server(struct http_uri* const uri, struct http_server_options* const opt) {
  struct https_server* const server = calloc(1, sizeof(struct https_server));
  if(server == NULL) {
    return -1;
  }
  server->tls.tcp.settings = opt->tcp_settings;
  server->tls.tcp.epoll = opt->epoll;
  server->tls.tcp.socket_size = opt->socket_size ? opt->socket_size : sizeof(struct https_serversock);
  server->tls.callbacks = &https_server_callbacks;
  server->tls.ctx = opt->ctx;
  server->context.default_settings = opt->default_settings ? opt->default_settings : &http_request_parser_settings;
  server->context.read_growth = opt->read_growth ? opt->read_growth : 16384;
  server->context.timeout_after = opt->timeout_after ? opt->timeout_after : 10;
  server->callbacks = opt->https_callbacks;
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
    server->context.alloc_table = 1;
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
      server->context.alloc_manager = 1;
    }
  }
  char hostname[256];
  (void) memcpy(hostname, uri->hostname, uri->hostname_len);
  hostname[uri->hostname_len] = 0;
  char port[6];
  if(uri->port_ptr != NULL) {
    (void) memcpy(port, uri->port_ptr, uri->port_len);
    port[uri->port_len] = 0;
  } else {
    (void) memcpy(port, "443", 4);
  }
  struct tls_server_options options = {
    .tcp = {
      opt->info,
      hostname,
      port,
      opt->family,
      opt->flags
    }
  };
  if(tls_server(&server->tls, &options) != 0) {
    goto err_time;
  }
  opt->https_server = server;
  return 0;
  
  err_time:
  if(server->context.alloc_manager) {
    time_manager_stop(server->context.manager);
    time_manager_free(server->context.manager);
    free(server->context.manager);
  }
  err_table:
  if(server->context.alloc_table) {
    http_hash_table_free(server->context.table);
    free(server->context.table);
  }
  err_serv:
  free(server);
  return -1;
}

void https_server_foreach_conn(struct https_server* const server, void (*callback)(struct https_serversock*, void*), void* data) {
  tls_server_foreach_conn(&server->tls, (void (*)(struct tls_socket*,void*)) callback, data);
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

#define socket ((struct https_serversock*) data)
#define server ((struct http_server*) socket->tls.tcp.server)

static void https_serversock_stop_socket(void* data) {
  socket->context.expected = 1;
  tls_socket_dont_receive_data(&socket->tls);
  http_serversock_timeout(&socket->tls.tcp);
  tls_socket_close(&socket->tls);
}

#undef server
#undef socket

static void https_server_free(struct https_server* const server) {
  if(server->context.alloc_manager) {
    time_manager_stop(server->context.manager);
    time_manager_free(server->context.manager);
    free(server->context.manager);
  }
  if(server->context.alloc_table) {
    http_hash_table_free(server->context.table);
    free(server->context.table);
  }
}

#define server ((struct https_server*) serv)

static int https_server_onnomem(struct tls_server* serv) {
  return server->callbacks->onnomem(server);
}

static void https_server_onerror(struct tls_server* serv) {
  if(server->callbacks->onerror != NULL) {
    server->callbacks->onerror(server);
  }
}

static void https_server_onshutdown(struct tls_server* serv) {
  server->callbacks->onshutdown(server);
  https_server_free(server);
  tls_server_free(&server->tls);
  free(server);
}

#undef server

#define socket ((struct https_serversock*) soc)
#define server ((struct https_server*) soc->tcp.server)

static int https_server_onconnection(struct tls_socket* soc, const struct sockaddr* addr) {
  socket->tls.tcp.settings = https_serversock_settings;
  socket->tls.callbacks = &https_serversock_callbacks;
  return 0;
}

static void https_serversock_onopen(struct tls_socket* soc) {
  (void) tcp_socket_keepalive(&socket->tls.tcp);
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
  if(socket->context.expected && socket->context.protocol != http_p_websocket) {
    return;
  }
  if(socket->context.protocol == http_p_websocket) {
    goto websocket;
  }
  socket->context.message.headers = socket->context.headers;
  socket->context.message.headers_len = 255;
  int state;
  if(socket->context.init_parsed == 0) {
    state = http1_parse_request(socket->tls.read_buffer, socket->tls.read_used, &socket->context.session, &http_prerequest_parser_settings, &socket->context.message);
    switch(state) {
      case http_incomplete: return;
      case http_valid: {
        const char save = socket->context.message.path[socket->context.message.path_len];
        socket->context.message.path[socket->context.message.path_len] = 0;
        socket->context.entry = http_hash_table_find(server->context.table, socket->context.message.path);
        socket->context.message.path[socket->context.message.path_len] = save;
        if(socket->context.entry != NULL) {
          if(socket->context.entry->data != NULL) {
            socket->context.settings = socket->context.entry->data;
          } else {
            socket->context.settings = server->context.default_settings;
          }
        } else {
          socket->context.backup_settings = *server->context.default_settings;
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
    state = http1_parse_request(socket->tls.read_buffer, socket->tls.read_used, &socket->context.session, socket->context.settings, &socket->context.message);
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
      connection = http1_seek_header(&socket->context.message, "connection", 10);
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
          ((void (*)(struct https_server*, struct https_serversock*, struct http_message*, struct http_message*)) socket->context.entry->func)(server, socket, &socket->context.message, &response);
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
    if(response.status_code != 0) {
#define add_header(a,b,c,d) response.headers[response.headers_len++]=(struct http_header){a,c,d,b,0,0}
      if(response.close_conn) {
        close_connection = 1;
        add_header("Connection", 10, "close", 5);
      } else if(connection != NULL) {
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
              if(server->context.compression_isnt_required) {
                goto out;
              }
              goto err_res;
            }
            if(server->callbacks->onnomem(server) == 0) {
              continue;
            }
            if(server->context.compression_isnt_required) {
              goto out;
            }
            goto err_res;
          }
        }
        if(response.alloc_body) {
          free(response.body);
        } else {
          response.alloc_body = 1;
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
      server->context.compression_isnt_required = 0;
      server->context.quality = 0;
      server->context.window_bits = 0;
      server->context.mem_level = 0;
      server->context.mode = 0;
      
      char content_length[21];
      int len = sprintf(content_length, "%" PRIu64, response.body_len);
      if(len < 0) {
        goto err_res;
      }
      add_header("Content-Length", 14, content_length, len);
      
      char date_header[30];
      date_header[0] = 0;
      struct tm tms;
      if(gmtime_r(&(time_t){time(NULL)}, &tms) != NULL) {
        const size_t len = strftime(date_header, 30, "%a, %d %b %Y %H:%M:%S GMT", &tms);
        if(len != 0) {
          add_header("Date", 4, date_header, len);
        }
      }
      
      char keepalive[30];
      if(!close_connection) {
        len = sprintf(keepalive, "timeout=%" PRIu64, server->context.timeout_after);
        if(len < 0) {
          goto err_res;
        }
        add_header("Keep-Alive", 10, keepalive, len);
      }
      add_header("Server", 6, "shnet", 5);
#undef add_header
      char* msg;
      const uint64_t length = http1_message_length(&response);
      while(1) {
        msg = malloc(length);
        if(msg == NULL) {
          if(server->callbacks->onnomem(server) == 0) {
            continue;
          }
          goto err_res;
        }
        break;
      }
      http1_create_message(msg, &response);
      (void) tls_send(&socket->tls, msg, length, tls_read_only);
    }
    if(response.alloc_body) {
      free(response.body);
    }
    for(uint32_t i = 0; i < response.headers_len; ++i) {
      if(response.headers[i].alloc_name) {
        free(response.headers[i].name);
      }
      if(response.headers[i].alloc_value) {
        free(response.headers[i].value);
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
    if(response.alloc_body) {
      free(response.body);
    }
    for(uint32_t i = 0; i < response.headers_len; ++i) {
      if(response.headers[i].alloc_name) {
        free(response.headers[i].name);
      }
      if(response.headers[i].alloc_value) {
        free(response.headers[i].value);
      }
    }
    goto err;
  }
  return;
  
  err:
  socket->context.expected = 1;
  (void) time_manager_cancel_timeout(server->context.manager, &socket->context.timeout);
  tcp_socket_force_close(&socket->tls.tcp);
  return;
  
  
  websocket:
  while(1) {
    while(1) {
      state = websocket_parse(socket->tls.read_buffer, socket->tls.read_used, &socket->context);
      if(state == http_out_of_memory) {
        if(server->callbacks->onnomem(server) == 0) {
          continue;
        }
        goto errr;
      }
      break;
    }
    if(state != http_incomplete) {
      if(state == http_valid) {
        if(socket->context.closing) {
          goto errr;
        }
        if(socket->context.message.opcode < 8) {
          socket->callbacks->onmessage(socket, socket->context.message.body, socket->context.message.body_len);
        } else if(socket->context.message.opcode == 8) {
          socket->context.closing = 1;
          if(socket->context.close_onreadclose) {
            (void) ws_send(socket, socket->context.message.body, socket->context.message.body_len > 1 ? 2 : 0, websocket_close, ws_dont_free);
            tcp_socket_close(&socket->tls.tcp);
          } else if(socket->callbacks->onreadclose != NULL) {
            socket->callbacks->onreadclose(socket, socket->context.message.close_code);
          }
        } else if(socket->context.message.opcode == 9) {
          (void) ws_send(socket, NULL, 0, websocket_pong, ws_dont_free);
        }
        if(socket->context.message.alloc_body) {
          free(socket->context.message.body);
        }
        socket->tls.read_used -= socket->context.session.chunk_idx + socket->context.session.last_idx;
        if(socket->tls.read_used != 0) {
          (void) memmove(socket->tls.read_buffer, socket->tls.read_buffer + socket->context.session.chunk_idx + socket->context.session.last_idx, socket->tls.read_used);
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
        socket->context.session = (struct http_parser_session){0};
        socket->context.message = (struct http_message){0};
      } else {
        errr:
        socket->context.expected = 1;
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
  if(!socket->context.expected) {
    (void) time_manager_cancel_timeout(server->context.manager, &socket->context.timeout);
  }
  if(socket->context.protocol == http_p_websocket) {
    socket->callbacks->onclose(socket, socket->context.message.close_code);
  }
  tls_socket_free(&socket->tls);
}

static void https_serversock_onfree(struct tls_socket* soc) {
  if(socket->context.read_buffer != NULL) {
    free(socket->context.read_buffer);
  }
  if(socket->context.deflater != NULL) {
    deflater_free(socket->context.deflater);
  }
  if(socket->context.deflater_mutex != NULL) {
    (void) pthread_mutex_destroy(socket->context.deflater_mutex);
    free(socket->context.deflater_mutex);
  }
  if(socket->context.inflater != NULL) {
    inflater_free(socket->context.inflater);
  }
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



int ws(void* const _net, const struct http_message* const request, struct http_message* const response, const int up) {
  struct http_serversock_context* context;
  if(((struct net_socket*) _net)->secure) {
    context = (struct http_serversock_context*)((char*) _net + sizeof(struct tls_socket));
  } else {
    context = (struct http_serversock_context*)((char*) _net + sizeof(struct tcp_socket));
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
    context->deflater_mutex = malloc(sizeof(pthread_mutex_t));
    if(context->deflater_mutex == NULL) {
      return -1;
    }
    const int err = pthread_mutex_init(context->deflater_mutex, NULL);
    if(err != 0) {
      errno = err;
      free(context->deflater_mutex);
      context->deflater_mutex = NULL;
      return -1;
    }
    unsigned char result[SHA_DIGEST_LENGTH];
    EVP_MD_CTX* ctx = EVP_MD_CTX_new();
    if(ctx == NULL) {
      goto err;
    }
    if(EVP_DigestInit_ex(ctx, EVP_sha1(), NULL) == 0 ||
       EVP_DigestUpdate(ctx, sec_key->value, 24) == 0 ||
       EVP_DigestUpdate(ctx, "258EAFA5-E914-47DA-95CA-C5AB0DC85B11", 36) == 0 ||
       EVP_DigestFinal_ex(ctx, result, NULL) == 0) {
      EVP_MD_CTX_free(ctx);
      goto err;
    }
    EVP_MD_CTX_free(ctx);
    char* const h = base64_encode(result, NULL, SHA_DIGEST_LENGTH);
    if(h == NULL) {
      goto err;
    }
    if(!context->settings->no_permessage_deflate) {
      const struct http_header* const deflate = http1_seek_header(request, "sec-websocket-extensions", 24);
      if(deflate != NULL) {
        const char save = deflate->value[deflate->value_len];
        deflate->value[deflate->value_len] = 0;
        context->permessage_deflate = strstr(deflate->value, "permessage-deflate") != NULL;
        deflate->value[deflate->value_len] = save;
      }
    }
    response->headers[response->headers_len++] = (struct http_header){"Connection","Upgrade",7,10,0,0};
    response->headers[response->headers_len++] = (struct http_header){"Upgrade","websocket",9,7,0,0};
    response->headers[response->headers_len++] = (struct http_header){"Sec-WebSocket-Accept",h,28,20,0,1};
    if(context->permessage_deflate) {
      response->headers[response->headers_len++] = (struct http_header){"Sec-WebSocket-Extensions","permessage-deflate",18,24,0,0};
    }
    response->headers[response->headers_len++] = (struct http_header){"Sec-WebSocket-Version","13",2,21,0,0};
    response->status_code = http_s_switching_protocols;
    response->reason_phrase = http1_default_reason_phrase(response->status_code);
    response->reason_phrase_len = http1_default_reason_phrase_len(response->status_code);
    context->session = (struct http_parser_session){0};
    context->protocol = http_p_websocket;
    if(((struct net_socket*) _net)->secure) {
      (void) time_manager_cancel_timeout(((struct https_server*)((struct https_serversock*) _net)->tls.tcp.server)->context.manager, &context->timeout);
    } else {
      (void) time_manager_cancel_timeout(((struct http_server*)((struct http_serversock*) _net)->tcp.server)->context.manager, &context->timeout);
    }
  }
  return 1;
  
  err:
  (void) pthread_mutex_destroy(context->deflater_mutex);
  free(context->deflater_mutex);
  context->deflater_mutex = NULL;
  return -1;
}

#define ATLEAST(a) \
do { \
  if((a) > size) return http_incomplete; \
} while(0)

int websocket_parse(void* _buffer, uint64_t size, struct http_serversock_context* const context) {
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
  if(opcode != 0) {
    context->message.opcode = opcode;
    if((opcode == 0 || (opcode & 8)) && context->message.encoding) {
      return http_invalid;
    }
  }
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
  if((context->message.opcode & 8) && payload_len > 125) {
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
  context->session.chunk_idx += header_len;
  (void) memmove(buffer - context->session.chunk_idx, buffer, payload_len);
  buffer -= context->session.chunk_idx;
  if(context->message.body == NULL) {
    context->message.body = (char*) buffer;
  }
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
  if(context->message.transfer) {
    if(context->message.opcode == 8 && context->message.body_len > 1) {
      context->message.close_code = ((unsigned char) context->message.body[0] << 8) | (unsigned char) context->message.body[1];
      return http_closed;
    }
    context->session.last_idx = context->message.body_len;
    if(context->message.encoding && context->message.body_len != 0) {
      if(context->inflater == NULL) {
        context->inflater = inflater(NULL, -Z_DEFLATE_DEFAULT_WINDOW_BITS);
        if(context->inflater == NULL) {
          return http_out_of_memory;
        }
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
      char* ptr = inflate_(context->inflater, context->message.body, context->message.body_len + 4, NULL, &len, context->settings->max_body_len, Z_SYNC_FLUSH);
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
      context->message.body = ptr;
      ptr = realloc(context->message.body, len);
      if(ptr != NULL) {
        context->message.body = ptr;
      }
      context->message.body_len = len;
      context->message.alloc_body = 1;
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

int ws_send(void* const _net, void* _buffer, uint64_t size, const uint8_t opcode, const uint8_t flags) {
  struct http_serversock_context* context;
  if(((struct net_socket*) _net)->secure) {
    context = (struct http_serversock_context*)((char*) _net + sizeof(struct tls_socket));
  } else {
    context = (struct http_serversock_context*)((char*) _net + sizeof(struct tcp_socket));
  }
  unsigned char* buffer = _buffer;
  unsigned char base[10] = {0};
  uint8_t alloc = 0;
  if(size > 0 && context->permessage_deflate) {
    size_t len = 0;
    alloc = 1;
    (void) pthread_mutex_lock(context->deflater_mutex);
    if(context->deflater == NULL) {
      context->deflater = deflater(NULL, Z_DEFAULT_COMPRESSION, -Z_DEFLATE_DEFAULT_WINDOW_BITS, Z_DEFAULT_MEM_LEVEL, Z_DEFAULT_STRATEGY);
      if(context->deflater == NULL) {
        (void) pthread_mutex_unlock(context->deflater_mutex);
        if(!(flags & ws_dont_free)) {
          free(_buffer);
        }
        return -1;
      }
    }
    buffer = deflate_(context->deflater, buffer, size, NULL, &len, Z_SYNC_FLUSH);
    (void) pthread_mutex_unlock(context->deflater_mutex);
    if(buffer == NULL) {
      if(!(flags & ws_dont_free)) {
        free(_buffer);
      }
      return -1;
    }
    if(!(flags & ws_dont_free)) {
      free(_buffer);
    }
    size = len - 4;
    len -= 4;
  }
  const uint64_t len = websocket_create_message_base(base, &((struct http_message) {
    .transfer = (flags & ws_not_last_frame) ? 0 : 1,
    .encoding = alloc ? 1 : 0,
    .opcode = opcode,
    .body_len = size
  }));
  if(((struct net_socket*) _net)->secure) {
    struct https_serversock* const socket = (struct https_serversock*) _net;
    if(tls_send(&socket->tls, base, len, tls_dont_free) != 0) {
      if(alloc) {
        free(buffer);
      }
      return -1;
    }
    if(size == 0) {
      return 0;
    }
    return tls_send(&socket->tls, buffer, size, alloc ? tcp_read_only : (flags & (ws_read_only | ws_dont_free)));
  } else {
    struct http_serversock* const socket = (struct http_serversock*) _net;
    if(tcp_send(&socket->tcp, base, len, tls_dont_free) != 0) {
      if(alloc) {
        free(buffer);
      }
      return -1;
    }
    if(size == 0) {
      return 0;
    }
    return tcp_send(&socket->tcp, buffer, size, alloc ? tcp_read_only : (flags & (ws_read_only | ws_dont_free)));
  }
}