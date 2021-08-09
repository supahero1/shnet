#include "tls.h"
#include "debug.h"
#include "aflags.h"

#include <errno.h>
#include <limits.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <openssl/err.h>

static int  tls_oncreation(struct tcp_socket*);

static void tls_onopen(struct tcp_socket*);

static void tls_onmessage(struct tcp_socket*);

static void tls_onsend(struct tcp_socket*);

static int  tls_socket_onnomem(struct tcp_socket*);

static void tls_onclose(struct tcp_socket*);

static void tls_onfree(struct tcp_socket*);


static int  tls_onconnection(struct tcp_socket*, const struct sockaddr*);

static int  tls_server_onnomem(struct tcp_server*);

static void tls_onerror(struct tcp_server*);

static void tls_onshutdown(struct tcp_server*);


static int  tls_send_internal(struct tls_socket* const, const void*, size_t);

static int  tls_send_buffered(struct tls_socket* const);

static uint64_t tls_read_internal(struct tls_socket* const);


static const struct tcp_socket_callbacks tls_socket_callbacks = {
  tls_oncreation,
  tls_onopen,
  tls_onmessage,
  NULL,
  tls_onsend,
  tls_socket_onnomem,
  tls_onclose,
  tls_onfree
};

static const struct tcp_server_callbacks tls_server_callbacks = {
  tls_onconnection,
  tls_server_onnomem,
  tls_onerror,
  tls_onshutdown
};

static const struct tls_socket_settings tls_socket_settings = { 0, 0, 0, 1 };

static const struct tcp_socket_settings tcp_socket_settings = { 1, 0, 0, 0, 0, 0, 1, 1 };

static const struct tcp_socket_settings tcp_socket_settings_reconnect = { 1, 0, 1, 1, 0, 1, 1, 1 };



static inline void tls_socket_set_flag(struct tls_socket* const socket, const uint32_t flag) {
  aflag_add2(&socket->tcp.flags, flag);
}

static inline uint32_t tls_socket_test_flag(const struct tls_socket* const socket, const uint32_t flag) {
  return aflag_test2(&socket->tcp.flags, flag);
}

static inline void tls_socket_clear_flag(struct tls_socket* const socket, const uint32_t flag) {
  aflag_del2(&socket->tcp.flags, flag);
}

static void tls_checksend(struct tls_socket* socket) {
  tls_socket_set_flag(socket, tls_wants_send);
  if(tls_socket_test_flag(socket, tcp_can_send)) {
    tls_socket_clear_flag(socket, tls_wants_send);
    (void) tls_send_internal(socket, NULL, 0);
  }
}



void tls_socket_free(struct tls_socket* const socket) {
  if(socket->alloc_ctx) {
    SSL_CTX_free(socket->ctx);
    socket->ctx = NULL;
    socket->alloc_ctx = 0;
  }
  tcp_socket_free(&socket->tcp);
}

void tls_socket_close(struct tls_socket* const socket) {
  (void) pthread_mutex_lock(&socket->tcp.lock);
  if(socket->tcp.send_len == 0) {
    tls_socket_force_close(socket);
  } else {
    tls_socket_set_flag(socket, tcp_closing);
  }
  (void) pthread_mutex_unlock(&socket->tcp.lock);
}

void tls_socket_force_close(struct tls_socket* const socket) {
  tls_socket_clear_flag(socket, tls_wants_send | tcp_can_send);
  (void) pthread_mutex_lock(&socket->ssl_lock);
  ERR_clear_error();
  const int status = SSL_shutdown(socket->ssl);
  if(status < 0) {
    const int err = SSL_get_error(socket->ssl, status);
    (void) pthread_mutex_unlock(&socket->ssl_lock);
    switch(err) {
      case SSL_ERROR_WANT_WRITE: {
        tls_checksend(socket);
      }
      default: break;
      case SSL_ERROR_SYSCALL:
      case SSL_ERROR_SSL: {
        tcp_socket_force_close(&socket->tcp);
        break;
      }
    }
  } else {
    (void) pthread_mutex_unlock(&socket->ssl_lock);
    if(status == 1) {
      tcp_socket_force_close(&socket->tcp);
    }
  }
}

void tls_socket_dont_receive_data(struct tls_socket* const socket) {
  tls_socket_set_flag(socket, tls_shutdown_rd);
}

void tls_socket_receive_data(struct tls_socket* const socket) {
  tls_socket_clear_flag(socket, tls_shutdown_rd);
}



static void tls_check(struct tls_socket* const socket) {
  if(socket->opened == 0 || socket->tcp.reconnecting) {
    (void) pthread_mutex_lock(&socket->ssl_lock);
    const int is_init_fin = SSL_is_init_finished(socket->ssl);
    (void) pthread_mutex_unlock(&socket->ssl_lock);
    if(is_init_fin) {
      tcp_socket_nodelay_off(&socket->tcp);
      tls_socket_set_flag(socket, tls_can_send);
      _debug("A", 1);
      (void) tls_send_buffered(socket);
      _debug("B", 1);
      if(socket->opened == 0) {
        socket->opened = 1;
        if(socket->callbacks->onopen != NULL) {
          socket->callbacks->onopen(socket);
        }
      } else if(socket->settings.onopen_when_reconnect) {
        socket->callbacks->onopen(socket);
      }
    }
  }
  if(socket->close_once == 0) {
    (void) pthread_mutex_lock(&socket->ssl_lock);
    const int shutdown = SSL_get_shutdown(socket->ssl);
    (void) pthread_mutex_unlock(&socket->ssl_lock);
    if(shutdown == (SSL_SENT_SHUTDOWN | SSL_RECEIVED_SHUTDOWN)) {
      /* We are done. Can even save the session. */
      socket->close_once = 1;
      socket->clean = 1;
      tcp_socket_force_close(&socket->tcp);
    } else if(shutdown == SSL_RECEIVED_SHUTDOWN) {
      socket->close_once = 1;
      socket->clean = 1;
      /* As RFC states:
      "Each party is required to send a close_notify alert before closing the
      write side of the connection. It is required that the other party respond
      with a close_notify alert of its own and close down the connection
      immediately, discarding any pending writes." */
      tls_socket_force_close(socket);
      tcp_socket_force_close(&socket->tcp);
    }
  }
}

#define socket ((struct tls_socket*) soc)

static int tls_oncreation(struct tcp_socket* soc) {
  if(socket->tcp.info != NULL && socket->tcp.info->ai_canonname == NULL && socket->tcp.addr != NULL && socket->tcp.addr->hostname != NULL) {
    const size_t len = strlen(socket->tcp.addr->hostname) + 1;
    socket->tcp.info->ai_canonname = malloc(len);
    if(socket->tcp.info->ai_canonname == NULL) {
      return -1;
    }
    (void) memcpy(socket->tcp.info->ai_canonname, socket->tcp.addr->hostname, len);
  }
  if(tls_socket_init(socket) != 0) {
    return -1;
  }
  if((socket->tcp.reconnecting && socket->settings.oncreation_when_reconnect) || (!socket->tcp.reconnecting && socket->callbacks->oncreation != NULL)) {
    return socket->callbacks->oncreation(socket);
  }
  return 0;
}

static void tls_onopen(struct tcp_socket* soc) {
  tcp_socket_nodelay_on(&socket->tcp);
  ERR_clear_error();
  switch(SSL_get_error(socket->ssl, SSL_do_handshake(socket->ssl))) {
    case SSL_ERROR_WANT_WRITE: {
      /* Do we really have so much to send that the TCP buffer gets full */
      tls_checksend(socket);
      break;
    }
    case SSL_ERROR_SYSCALL:
    case SSL_ERROR_SSL: {
      tcp_socket_force_close(&socket->tcp);
    }
    default: break;
  }
}

static void tls_onmessage(struct tcp_socket* soc) {
  const uint64_t read = tls_read_internal(socket);
  tls_check(socket);
  if(tls_socket_test_flag(socket, tls_shutdown_rd)) {
    if(socket->read_buffer != NULL) {
      free(socket->read_buffer);
      socket->read_buffer = NULL;
      socket->read_used = 0;
      socket->read_size = 0;
    }
  } else if(socket->callbacks->onmessage != NULL && read > 0) {
    socket->callbacks->onmessage(socket);
  }
}

static void tls_onsend(struct tcp_socket* soc) {
  if(tls_socket_test_flag(socket, tls_wants_send)) {
    (void) tls_send_internal(socket, NULL, 0);
  }
  _debug("yes", 1);
  if(tls_socket_test_flag(socket, tls_can_send | tcp_can_send) == (tls_can_send | tcp_can_send)) {
    _debug("tls sending buffered", 1);
    (void) tls_send_buffered(socket);
  }
}

static int tls_socket_onnomem(struct tcp_socket* soc) {
  return socket->callbacks->onnomem(socket);
}

static void tls_onclose(struct tcp_socket* soc) {
  tls_socket_clear_flag(socket, tls_can_send | tcp_can_send);
  socket->callbacks->onclose(socket);
}

static void tls_onfree(struct tcp_socket* soc) {
  if(socket->callbacks->onfree != NULL) {
    socket->callbacks->onfree(socket);
  }
  (void) pthread_mutex_destroy(&socket->read_lock);
  (void) pthread_mutex_destroy(&socket->ssl_lock);
  if(socket->alloc_ssl) {
    SSL_free(socket->ssl);
  }
  if(socket->alloc_ctx && !socket->tcp.reconnecting) {
    SSL_CTX_free(socket->ctx);
  }
  if(socket->read_buffer != NULL) {
    free(socket->read_buffer);
  }
  if(socket->tcp.server == NULL) {
    if(socket->alloc_ssl) {
      socket->ssl = NULL;
      socket->alloc_ssl = 0;
    }
    if(socket->alloc_ctx && !socket->tcp.reconnecting) {
      socket->ctx = NULL;
      socket->alloc_ctx = 0;
    }
    socket->read_buffer = NULL;
    socket->read_used = 0;
    socket->read_size = 0;
    socket->clean = 0;
    socket->opened = 0;
    socket->close_once = 0;
  }
}

#undef socket



SSL_CTX* tls_ctx(const char* const cert_path, const char* const key_path, const uintptr_t flags) {
  SSL_CTX* const ctx = SSL_CTX_new((flags & tls_client) ? TLS_client_method() : TLS_server_method());
  if(ctx == NULL) {
    return NULL;
  }
  const char* path;
  char cwd[PATH_MAX] = {0};
  size_t len = 0;
  if(cert_path != NULL && key_path != NULL) {
    if(cert_path[0] == '.') {
      /* Transform relative into absolute path */
      if(getcwd(cwd, PATH_MAX) == NULL) {
        goto err_ctx;
      }
      len = strlen(cwd);
      (void) memcpy(cwd + len, cert_path + 1, strlen(cert_path) - 1);
      path = cwd;
    } else {
      path = cert_path;
    }
    if(SSL_CTX_use_certificate_chain_file(ctx, path) != 1) {
      goto err_ctx;
    }
    if(key_path[0] == '.') {
      if(len == 0) {
        if(getcwd(cwd, PATH_MAX) == NULL) {
          goto err_ctx;
        }
        len = strlen(cwd);
      }
      (void) memcpy(cwd + len, key_path + 1, strlen(key_path));
      path = cwd;
    } else {
      path = key_path;
    }
    if(flags & tls_rsa_key) {
      if(SSL_CTX_use_RSAPrivateKey_file(ctx, path, SSL_FILETYPE_PEM) != 1) {
        goto err_ctx;
      }
    } else {
      if(SSL_CTX_use_PrivateKey_file(ctx, path, SSL_FILETYPE_PEM) != 1) {
        goto err_ctx;
      }
    }
    if(SSL_CTX_build_cert_chain(ctx, SSL_BUILD_CHAIN_FLAG_CHECK | SSL_BUILD_CHAIN_FLAG_IGNORE_ERROR | SSL_BUILD_CHAIN_FLAG_CLEAR_ERROR) == 0) {
      goto err_ctx;
    }
  }
  if(SSL_CTX_set_min_proto_version(ctx, TLS1_2_VERSION) == 0) {
    goto err_ctx;
  }
  return ctx;
  
  err_ctx:
  SSL_CTX_free(ctx);
  return NULL;
}

int tls_socket_init(struct tls_socket* const socket) {
  int err = pthread_mutex_init(&socket->read_lock, NULL);
  if(err != 0) {
    errno = err;
    return -1;
  }
  err = pthread_mutex_init(&socket->ssl_lock, NULL);
  if(err != 0) {
    errno = err;
    goto err_read_lock;
  }
  if(socket->read_growth == 0) {
    socket->read_growth = 16384;
  }
  if(socket->ssl == NULL) {
    socket->ssl = SSL_new(socket->ctx);
    if(socket->ssl == NULL) {
      goto err_ssl_lock;
    }
    socket->alloc_ssl = 1;
    if(SSL_set_fd(socket->ssl, socket->tcp.net.sfd) == 0) {
      goto err_ssl;
    }
    if(socket->tcp.info != NULL && socket->tcp.info->ai_canonname != NULL && SSL_set_tlsext_host_name(socket->ssl, socket->tcp.info->ai_canonname) == 0) {
      goto err_ssl;
    }
    if(socket->tcp.server == NULL) {
      SSL_set_connect_state(socket->ssl);
    } else {
      SSL_set_accept_state(socket->ssl);
    }
  }
  socket->tcp.net.secure = 1;
  return 0;
  
  err_ssl:
  if(socket->alloc_ssl) {
    SSL_free(socket->ssl);
    socket->ssl = NULL;
    socket->alloc_ssl = 0;
  }
  err_ssl_lock:
  (void) pthread_mutex_destroy(&socket->ssl_lock);
  err_read_lock:
  (void) pthread_mutex_destroy(&socket->read_lock);
  return -1;
}

int tls_socket(struct tls_socket* const socket, const struct tls_socket_options* const opt) {
  socket->tcp.callbacks = &tls_socket_callbacks;
  if(socket->settings.init == 0) {
    socket->settings = tls_socket_settings;
  }
  if(socket->tcp.settings.init == 0) {
    if(socket->settings.automatically_reconnect) {
      socket->tcp.settings = tcp_socket_settings_reconnect;
    } else {
      socket->tcp.settings = tcp_socket_settings;
    }
  }
  if(socket->ctx == NULL) {
    if(opt == NULL) {
      socket->ctx = tls_ctx(NULL, NULL, tls_client);
    } else {
      socket->ctx = tls_ctx(opt->cert_path, opt->key_path, opt->flags | tls_client);
    }
    if(socket->ctx == NULL) {
      return -1;
    }
    socket->alloc_ctx = 1;
  }
  if(tcp_socket(&socket->tcp, &opt->tcp) == -1) {
    if(socket->alloc_ctx) {
      SSL_CTX_free(socket->ctx);
      socket->alloc_ctx = 0;
    }
    return -1;
  }
  return 0;
}



static int tls_send_internal(struct tls_socket* const socket, const void* data, size_t size) {
  while(1) {
    errno = 0;
    size_t sent = 0;
    (void) pthread_mutex_lock(&socket->ssl_lock);
    ERR_clear_error();
    const int err = SSL_get_error(socket->ssl, SSL_write_ex(socket->ssl, data, size, &sent));
    (void) pthread_mutex_unlock(&socket->ssl_lock);
    tls_check(socket);
    switch(err) {
      case SSL_ERROR_NONE: {
        return 0;
      }
      case SSL_ERROR_ZERO_RETURN: {
        tls_socket_clear_flag(socket, tls_can_send | tcp_can_send);
        errno = EPIPE;
        return -2;
      }
      case SSL_ERROR_WANT_READ: {
        errno = 0;
        return -1;
      }
      case SSL_ERROR_WANT_WRITE: {
        tls_checksend(socket);
        errno = 0;
        return -1;
      }
      case SSL_ERROR_SYSCALL: {
        if(errno == EINTR || (errno == ENOMEM && socket->callbacks->onnomem(socket) == 0)) {
          continue;
        }
      }
      default:
      case SSL_ERROR_SSL: {
        tcp_socket_force_close(&socket->tcp);
        errno = -1;
        return -2;
      }
    }
  }
}

static int tls_send_buffered(struct tls_socket* const socket) {
  if(!tls_socket_test_flag(socket, tls_can_send | tcp_can_send)) {
    _debug("C", 1);
    return -1;
  }
  _debug("D", 1);
  (void) pthread_mutex_lock(&socket->tcp.lock);
  _debug("E", 1);
  if(socket->tcp.send_len != 0) {
    _debug("F", 1);
    while(1) {
      _debug("FF", 1);
      const int sent = tls_send_internal(socket, socket->tcp.send_queue->data, socket->tcp.send_queue->len);
      if(sent == 0) {
        _debug("FFF", 1);
        if(!socket->tcp.send_queue->dont_free) {
          free((void*) socket->tcp.send_queue->data);
        }
        --socket->tcp.send_len;
        (void) memmove(socket->tcp.send_queue, socket->tcp.send_queue + 1, sizeof(struct tcp_socket_send_frame) * socket->tcp.send_len);
        if(socket->tcp.send_len == 0) {
          break;
        }
      } else {
        _debug("FFFF", 1);
        return sent;
      }
    }
  }
  _debug("G", 1);
  (void) pthread_mutex_unlock(&socket->tcp.lock);
  _debug("H", 1);
  if(tls_socket_test_flag(socket, tcp_closing)) {
    _debug("I", 1);
    tls_socket_force_close(socket);
  }
  return 0;
}

/* tls_send() returns 0 on success and -1 on failure. It might set errno to an
error code.

If errno is -1, a fatal OpenSSL error occured and the connection is closing.

Most applications can ignore the return value and errno. */

int tls_send(struct tls_socket* const socket, const void* data, uint64_t size, const int flags) {
  if(tls_socket_test_flag(socket, tcp_shutdown_wr) || tls_socket_test_flag(socket, tls_can_send | tcp_can_send) != (tls_can_send | tcp_can_send)) {
    if(!socket->tcp.settings.automatically_reconnect) {
      errno = EPIPE;
      goto err0;
    }
    return tcp_buffer(&socket->tcp, data, size, 0, flags);
  }
  int err = tls_send_buffered(socket);
  if(err == -2) {
    goto err1;
  }
  if(err == 0) {
    err = tls_send_internal(socket, data, size);
    if(err == -2) {
      goto err1;
    } else if(err == 0) {
      goto err0;
    }
  }
  return tcp_buffer(&socket->tcp, data, size, 0, flags);
  
  err1:
  if(!(flags & tls_dont_free)) {
    free((void*) data);
  }
  return -1;
  
  err0:
  if(!(flags & tls_dont_free)) {
    free((void*) data);
  }
  return 0;
}



static uint64_t tls_read_internal(struct tls_socket* const socket) {
  uint64_t addon = 0;
  while(1) {
    (void) pthread_mutex_lock(&socket->read_lock);
    if(socket->read_used + socket->read_growth + addon > socket->read_size) {
      while(1) {
        char* const ptr = realloc(socket->read_buffer, socket->read_used + socket->read_growth + addon);
        /* Here we can't ignore OOM. Either free memory or kill the socket. We
        MUST expand the read buffer, because else we wouldn't be able to process
        the incoming message, whatever it would be. There is no delaying - if we
        can't do it now, we won't get an EPOLLIN again, since this is edge
        triggered. Thus, we can't afford dropping any messages or such. */
        if(ptr != NULL) {
          socket->read_buffer = ptr;
          socket->read_size = socket->read_used + socket->read_growth + addon;
          break;
        } else if(socket->callbacks->onnomem(socket) == 0) {
          continue;
        } else {
          (void) pthread_mutex_unlock(&socket->read_lock);
          errno = ENOMEM;
          return 0;
        }
      }
    }
    (void) pthread_mutex_unlock(&socket->read_lock);
    errno = 0;
    size_t read = 0;
    (void) pthread_mutex_lock(&socket->ssl_lock);
    ERR_clear_error();
    const int err = SSL_get_error(socket->ssl, SSL_read_ex(socket->ssl, socket->read_buffer + socket->read_used + addon, socket->read_size - socket->read_used - addon, &read));
    (void) pthread_mutex_unlock(&socket->ssl_lock);
    switch(err) {
      case SSL_ERROR_NONE: {
        addon += read;
        /* SSL_read() can return data in parts, thus we need to continue reading */
        continue;
      }
      case SSL_ERROR_ZERO_RETURN: {
        errno = EPIPE;
        break;
      }
      case SSL_ERROR_WANT_READ: {
        /* We have read everything we could */
        errno = 0;
        break;
      }
      case SSL_ERROR_WANT_WRITE: {
        tls_checksend(socket);
        errno = 0;
        break;
      }
      case SSL_ERROR_SYSCALL: {
        if(errno == EINTR) {
          continue;
        }
      }
      default:
      case SSL_ERROR_SSL: {
        tcp_socket_force_close(&socket->tcp);
        break;
      }
    }
    (void) pthread_mutex_lock(&socket->read_lock);
    socket->read_used += addon;
    (void) pthread_mutex_unlock(&socket->read_lock);
    return addon;
  }
}

uint64_t tls_read(struct tls_socket* const socket, void* const data, const uint64_t size) {
  (void) pthread_mutex_lock(&socket->read_lock);
  const uint64_t to_read = size > socket->read_used ? socket->read_used : size;
  (void) memcpy(data, socket->read_buffer, to_read);
  socket->read_used -= to_read;
  if(socket->read_used != 0) {
    if(socket->read_size - socket->read_used >= socket->read_growth) {
      char* const ptr = realloc(socket->read_buffer, socket->read_used + socket->read_growth);
      if(ptr != NULL) {
        socket->read_buffer = ptr;
        socket->read_size = socket->read_used + socket->read_growth;
      }
    }
  } else {
    free(socket->read_buffer);
    socket->read_buffer = NULL;
    socket->read_used = 0;
    socket->read_size = 0;
  }
  (void) pthread_mutex_unlock(&socket->read_lock);
  return to_read;
}

/* Usually, the application will likely want to use tls_read() so that lock
contention doesn't become a problem when calling tls_peek_once() repetively, or
when locking the mutex manually and then calling tls_peek() for greater speed.
The underlying code might want to write some bytes to the read buffer at any
time, so well-written applications will probably have better performance by not
creating many lock contention opportunities and using tls_read() to copy memory. */

unsigned char tls_peek(const struct tls_socket* const socket, const uint64_t offset) {
  return socket->read_buffer[offset];
}

unsigned char tls_peek_once(struct tls_socket* const socket, const uint64_t offset) {
  (void) pthread_mutex_lock(&socket->read_lock);
  const unsigned char byte = tls_peek(socket, offset);
  (void) pthread_mutex_unlock(&socket->read_lock);
  return byte;
}



#define socket ((struct tls_socket*) sock)
#define server ((struct tls_server*) sock->server)

static int tls_onconnection(struct tcp_socket* sock, const struct sockaddr* addr) {
  socket->ctx = server->ctx;
  socket->tcp.callbacks = &tls_socket_callbacks;
  if(tls_socket_init(socket) != 0) {
    return -1;
  }
  const int err = server->callbacks->onconnection(socket, addr);
  if(err == 0 && socket->settings.init == 0) {
    socket->settings = tls_socket_settings;
  }
  return err;
}

#undef server
#undef socket

#define server ((struct tls_server*) serv)

static int tls_server_onnomem(struct tcp_server* serv) {
  return server->callbacks->onnomem(server);
}

static void tls_onerror(struct tcp_server* serv) {
  if(server->callbacks->onerror != NULL) {
    server->callbacks->onerror(server);
  }
}

static void tls_onshutdown(struct tcp_server* serv) {
  server->callbacks->onshutdown(server);
}

#undef server



void tls_server_free(struct tls_server* const server) {
  if(server->tcp.alloc_ctx) {
    SSL_CTX_free(server->ctx);
    server->ctx = NULL;
    server->tcp.alloc_ctx = 0;
  }
  tcp_server_free(&server->tcp);
}

int tls_server(struct tls_server* const server, struct tls_server_options* const opt) {
  if(server->tcp.socket_size == 0) {
    server->tcp.socket_size = sizeof(struct tls_socket);
  }
  server->tcp.callbacks = &tls_server_callbacks;
  if(server->ctx == NULL) {
    server->ctx = tls_ctx(opt->cert_path, opt->key_path, opt->flags);
    if(server->ctx == NULL) {
      goto err_sock;
    }
    server->tcp.alloc_ctx = 1;
  }
  if(tcp_server(&server->tcp, &opt->tcp) == -1) {
    goto err_ctx;
  }
  server->tcp.net.net.secure = 1;
  return 0;
  
  err_ctx:
  if(server->tcp.alloc_ctx) {
    SSL_CTX_free(server->ctx);
    server->ctx = NULL;
    server->tcp.alloc_ctx = 0;
  }
  err_sock:
  server->tcp.socket_size = 0;
  return -1;
}

void tls_server_foreach_conn(struct tls_server* const server, void (*callback)(struct tls_socket*, void*), void* data) {
  tcp_server_foreach_conn(&server->tcp, (void (*)(struct tcp_socket*,void*)) callback, data);
}

void tls_server_dont_accept_conn(struct tls_server* const server) {
  tcp_server_dont_accept_conn(&server->tcp);
}

void tls_server_accept_conn(struct tls_server* const server) {
  tcp_server_accept_conn(&server->tcp);
}

int tls_server_shutdown(struct tls_server* const server) {
  return tcp_server_shutdown(&server->tcp);
}

uint32_t tls_server_get_conn_amount_raw(const struct tls_server* const server) {
  return tcp_server_get_conn_amount_raw(&server->tcp);
}

uint32_t tls_server_get_conn_amount(struct tls_server* const server) {
  return tcp_server_get_conn_amount(&server->tcp);
}

int tls_epoll(struct net_epoll* const epoll) {
  return tcp_epoll(epoll);
}

void tls_ignore_sigpipe() {
  (void) signal(SIGPIPE, SIG_IGN);
}

void tls_get_OpenSSL_error(char* const buffer, const size_t size) {
  const unsigned long err = ERR_get_error();
  if(err == 0) {
    return;
  }
  ERR_error_string_n(err, buffer, size);
}