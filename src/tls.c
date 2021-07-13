#include "tls.h"
#include "aflags.h"

#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <openssl/err.h>

static int tls_oncreation(struct tcp_socket*);

static void tls_onopen(struct tcp_socket*);

static void tls_onmessage(struct tcp_socket*);

static void tls_onreadclose(struct tcp_socket*);

static void tls_onsend(struct tcp_socket*);

static int tls_socket_onnomem(struct tcp_socket*);

static void tls_onclose(struct tcp_socket*);

static void tls_onfree(struct tcp_socket*);


static int tls_onconnection(struct tcp_socket*);

static int tls_server_onnomem(struct tcp_server*);

static void tls_onshutdown(struct tcp_server*);


static int tls_internal_send(struct tls_socket* const, const void*, int);

static int tls_internal_read(struct tls_socket* const);


static struct tcp_socket_callbacks tls_tcp_socket_callbacks;

static struct tcp_server_callbacks tls_tcp_server_callbacks;

static _Atomic int __tls_initialised = 0;

static void __tls_init() {
  if(atomic_compare_exchange_strong(&__tls_initialised, &(int){0}, 1)) {
    tls_tcp_socket_callbacks = (struct tcp_socket_callbacks) {
      tls_oncreation,
      tls_onopen,
      tls_onmessage,
      tls_onreadclose,
      tls_onsend,
      tls_socket_onnomem,
      tls_onclose,
      tls_onfree
    };
    tls_tcp_server_callbacks = (struct tcp_server_callbacks) {
      tls_onconnection,
      tls_server_onnomem,
      tls_onshutdown
    };
  }
}



static inline void tls_socket_set_flag(struct tls_socket* const socket, const uint32_t flag) {
  aflag32_add(&socket->tcp.flags, flag);
}

static inline uint32_t tls_socket_test_flag(const struct tls_socket* const socket, const uint32_t flag) {
  return aflag32_test(&socket->tcp.flags, flag);
}

static inline void tls_socket_clear_flag(struct tls_socket* const socket, const uint32_t flag) {
  aflag32_del(&socket->tcp.flags, flag);
}



void tls_socket_free(struct tls_socket* const socket) {
  tcp_socket_free(&socket->tcp);
}

void tls_socket_close(struct tls_socket* const socket) {
  tls_socket_set_flag(socket, tls_shutdown_wr);
  tls_socket_clear_flag(socket, tls_wants_send);
  (void) pthread_mutex_lock(&socket->ssl_lock);
  ERR_clear_error();
  const int status = SSL_shutdown(socket->ssl);
  if(status < 0) {
    const int err = SSL_get_error(socket->ssl, status);
    (void) pthread_mutex_unlock(&socket->ssl_lock);
    switch(err) {
      case SSL_ERROR_WANT_WRITE: {
        tls_socket_set_flag(socket, tls_wants_send);
      }
      default: break;
      case SSL_ERROR_SYSCALL:
      case SSL_ERROR_SSL: {
        /* If we can't shutdown TLS, we need to do it on TCP level */
        if(socket->settings->force_close_on_shutdown_error) {
          tls_socket_force_close(socket);
        } else {
          tcp_socket_close(&socket->tcp);
        }
        break;
      }
    }
  } else {
    (void) pthread_mutex_unlock(&socket->ssl_lock);
    if(status == 1) {
      if(socket->settings->force_close_tcp) {
        tls_socket_force_close(socket);
      } else {
        tcp_socket_close(&socket->tcp);
      }
    }
  }
}

void tls_socket_force_close(struct tls_socket* const socket) {
  tcp_socket_force_close(&socket->tcp);
}



#define socket ((struct tls_socket*) soc)

static int tls_oncreation(struct tcp_socket* soc) {
  if(tls_socket_init(socket, net_socket) != 0) {
    return -1;
  }
  if(socket->callbacks->oncreation != NULL) {
    return socket->callbacks->oncreation(socket);
  }
  return 0;
}

static void tls_onopen(struct tcp_socket* soc) {
  if(socket->tcp.server != NULL) {
    return;
  }
  ERR_clear_error();
  switch(SSL_get_error(socket->ssl, SSL_do_handshake(socket->ssl))) {
    case SSL_ERROR_WANT_WRITE: {
      tls_socket_set_flag(socket, tls_wants_send);
      break;
    }
    case SSL_ERROR_SYSCALL:
    case SSL_ERROR_SSL: {
      if(socket->settings->force_close_on_fatal_error == 1) {
        tls_socket_force_close(socket);
      } else {
        tcp_socket_close(&socket->tcp);
      }
    }
    default: break;
  }
}

static void tls_onmessage(struct tcp_socket* soc) {
  {
    const int read = tls_internal_read(socket);
    if(SSL_is_init_finished(socket->ssl) && !tls_socket_test_flag(socket, tls_opened)) {
      tls_socket_set_flag(socket, tls_opened);
      if(socket->callbacks->onopen != NULL) {
        socket->callbacks->onopen(socket);
      }
    }
    if(read > 0 && socket->callbacks->onmessage != NULL) {
      socket->callbacks->onmessage(socket);
    }
  }
  if(!tls_socket_test_flag(socket, tls_onreadclose_once)) {
    (void) pthread_mutex_lock(&socket->ssl_lock);
    const int shutdown = SSL_get_shutdown(socket->ssl);
    (void) pthread_mutex_unlock(&socket->ssl_lock);
    if(shutdown == (SSL_SENT_SHUTDOWN | SSL_RECEIVED_SHUTDOWN)) {
      tls_socket_set_flag(socket, tls_onreadclose_once);
      if(socket->settings->force_close_tcp) {
        tls_socket_force_close(socket);
      } else {
        tcp_socket_close(&socket->tcp);
      }
    } else if(shutdown == SSL_RECEIVED_SHUTDOWN) {
      tls_socket_set_flag(socket, tls_onreadclose_once);
      switch(socket->settings->onreadclose_auto_res) {
        case tls_onreadclose_callback: {
          socket->callbacks->tls_onreadclose(socket);
          break;
        }
        case tls_onreadclose_tls_close: {
          tls_socket_close(socket);
          break;
        }
        case tls_onreadclose_tcp_close: {
          tcp_socket_close(&socket->tcp);
          break;
        }
        case tls_onreadclose_tcp_force_close: {
          tls_socket_force_close(socket);
          break;
        }
        case tls_onreadclose_do_nothing: break;
      }
    }
  }
}

static void tls_onreadclose(struct tcp_socket* soc) {
  if(socket->callbacks->tcp_onreadclose != NULL) {
    socket->callbacks->tcp_onreadclose(socket);
  }
}

static void tls_onsend(struct tcp_socket* soc) {
  if(tls_socket_test_flag(socket, tls_wants_send)) {
    (void) tls_internal_send(socket, NULL, 0);
  }
  (void) pthread_mutex_lock(&socket->ssl_lock);
  const int is_finished = SSL_is_init_finished(socket->ssl);
  (void) pthread_mutex_unlock(&socket->ssl_lock);
  if(is_finished && !tls_socket_test_flag(socket, tls_opened)) {
    tls_socket_set_flag(socket, tls_opened);
    if(socket->callbacks->onopen != NULL) {
      socket->callbacks->onopen(socket);
    }
  }
}

static int tls_socket_onnomem(struct tcp_socket* soc) {
  return socket->callbacks->onnomem(socket);
}

static void tls_onclose(struct tcp_socket* soc) {
  socket->callbacks->onclose(socket);
}

static void tls_onfree(struct tcp_socket* soc) {
  if(socket->callbacks->onfree != NULL) {
    socket->callbacks->onfree(socket);
  }
  (void) pthread_mutex_destroy(&socket->read_lock);
  (void) pthread_mutex_destroy(&socket->ssl_lock);
  SSL_free(socket->ssl);
  if(socket->read_buffer != NULL) {
    free(socket->read_buffer);
  }
  if(socket->tcp.server != NULL) {
    (void) memset(soc + 1, 0, sizeof(struct tls_socket) - sizeof(struct tcp_socket));
  } else {
    const int offset = offsetof(struct tls_socket, ssl);
    (void) memset((char*) soc + offset, 0, sizeof(struct tls_socket) - offset);
  }
}

#undef socket



int tls_socket_init(struct tls_socket* const socket, const int which) {
  int err = pthread_mutex_init(&socket->read_lock, NULL);
  if(err != 0) {
    errno = err;
    return -1;
  }
  err = pthread_mutex_init(&socket->ssl_lock, NULL);
  if(err != 0) {
    errno = err;
    goto err_rl;
  }
  socket->ssl = SSL_new(socket->ctx);
  if(socket->ssl == NULL) {
    goto err_sl;
  }
  if(SSL_set_fd(socket->ssl, socket->tcp.base.sfd) == 0) {
    goto err_s;
  }
  if(which == net_socket) {
    SSL_set_connect_state(socket->ssl);
  } else {
    SSL_set_accept_state(socket->ssl);
  }
  /* Required to shrink and grow the buffer of records to be sent */
  SSL_set_mode(socket->ssl, SSL_MODE_ACCEPT_MOVING_WRITE_BUFFER);
  return 0;
  
  err_s:
  SSL_free(socket->ssl);
  err_sl:
  (void) pthread_mutex_destroy(&socket->ssl_lock);
  err_rl:
  (void) pthread_mutex_destroy(&socket->read_lock);
  return -1;
}

int tls_create_socket(struct tls_socket* const socket) {
  __tls_init();
  socket->tcp.callbacks = &tls_tcp_socket_callbacks;
  return tcp_create_socket(&socket->tcp);
}



static int tls_internal_send(struct tls_socket* const socket, const void* data, int size) {
  while(1) {
    ERR_clear_error();
    errno = 0;
    size_t sent = 0;
    (void) pthread_mutex_lock(&socket->ssl_lock);
    const int err = SSL_get_error(socket->ssl, SSL_write_ex(socket->ssl, data, size, &sent));
    (void) pthread_mutex_unlock(&socket->ssl_lock);
    switch(err) {
      case SSL_ERROR_NONE: {
        return 1;
      }
      case SSL_ERROR_ZERO_RETURN: {
        errno = EPIPE;
        return -1;
      }
      case SSL_ERROR_WANT_READ: {
        errno = 0;
        return 0;
      }
      case SSL_ERROR_WANT_WRITE: {
        tls_socket_set_flag(socket, tls_wants_send);
        tls_socket_clear_flag(socket, tcp_can_send);
        errno = 0;
        return 0;
      }
      case SSL_ERROR_SYSCALL: {
        if(errno == EINTR) {
          continue;
        }
      }
      default:
      case SSL_ERROR_SSL: {
        if(socket->settings->force_close_on_fatal_error == 1) {
          tls_socket_force_close(socket);
        } else {
          /* We MUST NOT use tls_socket_close(), according to the OpenSSL documentation */
          tcp_socket_close(&socket->tcp);
        }
        errno = -1;
        return -1;
      }
    }
  }
}

/* tls_send returns 1 if it managed to send everything. If the value is 0, errno
might be set to indicate the error, but it will not always be.
The application must not repeat the call if errno was ENOMEM, since that means
onnomem() callback failed, meaning the socket is closing.
errno -1 means a fatal OpenSSL error has occured. */

int tls_send(struct tls_socket* const socket, const void* data, int size) {
  if(tls_socket_test_flag(socket, tls_shutdown_wr) || tls_socket_test_flag(socket, tcp_shutdown_wr)) {
    errno = EPIPE;
    return 0;
  }
  (void) pthread_mutex_lock(&socket->tcp.lock);
  /* Send buffered records first, if possible. This way, there will be no dumb
  recordering if we first try to send the requested data and epoll has not yet
  woken up to notice it can send the buffered records. */
  int addon = sizeof(struct tls_record);
#define record ((struct tls_record*) socket->tcp.send_buffer)
  for(unsigned i = 0; i < socket->tcp.send_used; ++i) {
    switch(tls_internal_send(socket, socket->tcp.send_buffer + addon, record->size)) {
      case 1: {
        addon += record->total_size;
        break;
      }
      case 0: {
        goto breakout;
      }
      case -1: {
        (void) pthread_mutex_unlock(&socket->tcp.lock);
        return 0;
      }
    }
  }
#undef record
  breakout:
  addon -= sizeof(struct tls_record);
  socket->tcp.send_used -= addon;
  if(socket->tcp.send_used != 0) {
    (void) memmove(socket->tcp.send_buffer, socket->tcp.send_buffer + addon, socket->tcp.send_used);
    if(socket->tcp.send_size - socket->tcp.send_used >= socket->tcp.settings->send_buffer_cleanup_threshold) {
      char* const ptr = realloc(socket->tcp.send_buffer, socket->tcp.send_used);
      /* Yes, we could call onnomem() here, but then, when do we stop calling it
      if we keep getting OOM? Shrinking the buffer isn't anything that we require
      anyway, so we will be better off not calling onnomem() here. */
      if(ptr != NULL) {
        socket->tcp.send_buffer = ptr;
        socket->tcp.send_size = socket->tcp.send_used;
      }
    }
  } else {
    free(socket->tcp.send_buffer);
    socket->tcp.send_buffer = NULL;
    socket->tcp.send_size = 0;
  }
  (void) pthread_mutex_unlock(&socket->tcp.lock);
  /* After we've dealt with the buffered records and we can send more, try sending
  requested data. If can't do it, create and buffer a new record. */
  switch(tls_internal_send(socket, data, size)) {
    case 0: {
      /* Buffer the data */
      (void) pthread_mutex_lock(&socket->tcp.lock);
      const unsigned total_size = (size + sizeof(struct tls_record) - 1) & -sizeof(struct tls_record);
      if(socket->tcp.send_size - socket->tcp.send_used < total_size) {
        while(1) {
          char* const ptr = realloc(socket->tcp.send_buffer, socket->tcp.send_used + total_size);
          if(ptr != NULL) {
            socket->tcp.send_buffer = ptr;
            socket->tcp.send_size = socket->tcp.send_used + total_size;
            break;
          } else if(socket->callbacks->onnomem(socket) == 0) {
            continue;
          } else {
            (void) pthread_mutex_unlock(&socket->tcp.lock);
            errno = ENOMEM;
            return 0;
          }
        }
      }
      *((struct tls_record*)(socket->tcp.send_buffer + socket->tcp.send_used)) = (struct tls_record) {
        .size = size,
        .total_size = total_size
      };
      (void) memcpy(socket->tcp.send_buffer + socket->tcp.send_used + sizeof(struct tls_record), data, size);
      socket->tcp.send_used += total_size;
      (void) pthread_mutex_unlock(&socket->tcp.lock);
    }
    case 1: {
      /* We were able to send the record - best case scenario */
      errno = 0;
      return 1;
    }
    default: /* To silence no return warning */
    case -1: {
      /* A fatal error, tell the application we couldn't send anything and wait
      for the next epoll iteration to close the socket */
      errno = 0;
      return 0;
    }
  }
}



static int tls_internal_read(struct tls_socket* const socket) {
  if(tls_socket_test_flag(socket, tcp_data_ended)) {
    errno = EPIPE;
    return 0;
  }
  int addon = 0;
  while(1) {
    (void) pthread_mutex_lock(&socket->read_lock);
    if(socket->read_used + socket->settings->read_buffer_growth + addon > socket->read_size) {
      while(1) {
        char* const ptr = realloc(socket->read_buffer, socket->read_used + socket->settings->read_buffer_growth + addon);
        /* Here we can't ignore OOM. Either free memory or kill the socket. We
        MUST expand the read buffer, because else we wouldn't be able to process
        the incoming message, whatever it would be. There is no delaying - if we
        can't do it now, we won't get an EPOLLIN again, since this is edge
        triggered. Thus, we can't afford dropping any messages or such. */
        if(ptr != NULL) {
          socket->read_buffer = ptr;
          socket->read_size = socket->read_used + socket->settings->read_buffer_growth + addon;
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
    ERR_clear_error();
    errno = 0;
    size_t read = 0;
    (void) pthread_mutex_lock(&socket->ssl_lock);
    const int err = SSL_get_error(socket->ssl, SSL_read_ex(socket->ssl, socket->read_buffer + addon, socket->read_size - socket->read_used - addon, &read));
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
        tls_socket_set_flag(socket, tls_wants_send);
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
        if(socket->settings->force_close_on_fatal_error == 1) {
          tls_socket_force_close(socket);
        } else {
          /* We MUST NOT use tls_socket_close(), according to the OpenSSL documentation */
          tcp_socket_close(&socket->tcp);
        }
        break;
      }
    }
    (void) pthread_mutex_lock(&socket->read_lock);
    socket->read_used += addon;
    (void) pthread_mutex_unlock(&socket->read_lock);
    return addon;
  }
}

int tls_read(struct tls_socket* const socket, void* const data, const int size) {
  (void) pthread_mutex_lock(&socket->read_lock);
  const int to_read = size > socket->read_used ? socket->read_used : size;
  (void) memcpy(data, socket->read_buffer, to_read);
  socket->read_used -= to_read;
  if(socket->read_used != 0) {
    if(socket->read_size - socket->read_used >= socket->settings->read_buffer_cleanup_threshold) {
      char* const ptr = realloc(socket->read_buffer, socket->read_used);
      if(ptr != NULL) {
        socket->read_buffer = ptr;
        socket->read_size = socket->read_used;
      }
    }
  } else {
    free(socket->read_buffer);
    socket->read_buffer = NULL;
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

unsigned char tls_peek(const struct tls_socket* const socket, const int offset) {
  return socket->read_buffer[offset];
}

unsigned char tls_peek_once(struct tls_socket* const socket, const int offset) {
  (void) pthread_mutex_lock(&socket->read_lock);
  const unsigned char byte = tls_peek(socket, offset);
  (void) pthread_mutex_unlock(&socket->read_lock);
  return byte;
}



#define socket ((struct tls_socket*) sock)
#define server ((struct tls_server*) sock->server)

static int tls_onconnection(struct tcp_socket* sock) {
  socket->ctx = server->ctx;
  __tls_init();
  socket->tcp.callbacks = &tls_tcp_socket_callbacks;
  return server->callbacks->onconnection(socket);
}

#undef server
#undef socket

#define server ((struct tls_server*) serv)

static int tls_server_onnomem(struct tcp_server* serv) {
  return server->callbacks->onnomem(server);
}

static void tls_onshutdown(struct tcp_server* serv) {
  server->callbacks->onshutdown(server);
}

#undef server



void tls_server_free(struct tls_server* const server) {
  tcp_server_free(&server->tcp);
}

int tls_create_server(struct tls_server* const server, struct addrinfo* const info) {
  if(server->tcp.settings->socket_size == 0) {
    server->tcp.settings->socket_size = sizeof(struct tls_socket);
  }
  server->tcp.callbacks = &tls_tcp_server_callbacks;
  return tcp_create_server(&server->tcp, info);
}

void tls_server_foreach_conn(struct tls_server* const server, void (*callback)(struct tls_socket*, void*), void* data, const int write) {
  tcp_server_foreach_conn(&server->tcp, (void (*)(struct tcp_socket*,void*)) callback, data, write);
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

unsigned tls_server_get_conn_amount_raw(const struct tls_server* const server) {
  return tcp_server_get_conn_amount_raw(&server->tcp);
}

unsigned tls_server_get_conn_amount(struct tls_server* const server) {
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