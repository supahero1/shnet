#include "tls.h"
#include "aflags.h"

#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <openssl/err.h>

static inline void tls_socket_set_flag(struct tls_socket* const socket, const uint32_t flag) {
  aflag32_add(&socket->flags, flag);
}

static inline uint32_t tls_socket_test_flag(const struct tls_socket* const socket, const uint32_t flag) {
  return aflag32_test(&socket->flags, flag);
}

static inline void tls_socket_clear_flag(struct tls_socket* const socket, const uint32_t flag) {
  aflag32_del(&socket->flags, flag);
}



void tls_socket_free(struct tls_socket* const socket) {
  tcp_socket_free((struct tcp_socket*) socket);
}

void tls_socket_close(struct tls_socket* const socket) {
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
        if(socket->tls_settings->force_close_on_shutdown_error) {
          tls_socket_force_close(socket);
        } else {
          tcp_socket_close((struct tcp_socket*) socket);
        }
        break;
      }
    }
  } else {
    (void) pthread_mutex_unlock(&socket->ssl_lock);
    if(status == 1) {
      if(socket->tls_settings->force_close_tcp) {
        tls_socket_force_close(socket);
      } else {
        tcp_socket_close((struct tcp_socket*) socket);
      }
    }
  }
}

void tls_socket_force_close(struct tls_socket* const socket) {
  tcp_socket_force_close((struct tcp_socket*) socket);
}



static int tls_internal_send(struct tls_socket* const, const void*, int);

static int tls_internal_read(struct tls_socket* const);

#define socket ((struct tls_socket*) soc)

int tls_oncreation(struct tcp_socket* soc) {
  if(tls_socket_init(socket, net_socket) != 0) {
    return -1;
  }
  if(socket->tls_callbacks->oncreation != NULL) {
    return socket->tls_callbacks->oncreation(socket);
  }
  return 0;
}

void tls_onopen(struct tcp_socket* soc) {
  if(socket->server != NULL) {
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
      if(socket->tls_settings->force_close_on_fatal_error == 1) {
        tls_socket_force_close(socket);
      } else {
        tcp_socket_close((struct tcp_socket*) socket);
      }
      break;
    }
    default: break;
  }
}

void tls_onmessage(struct tcp_socket* soc) {
  {
    const int read = tls_internal_read(socket);
    if(SSL_is_init_finished(socket->ssl) && !tls_socket_test_flag(socket, tls_opened)) {
      tls_socket_set_flag(socket, tls_opened);
      if(socket->tls_callbacks->onopen != NULL) {
        socket->tls_callbacks->onopen(socket);
      }
    }
    if(read > 0 && socket->tls_callbacks->onmessage != NULL) {
      socket->tls_callbacks->onmessage(socket);
    }
  }
  /* We can do this safely, because sockets can only be in 1 epoll at a time, and
  the SSL argument of SSL_get_shutdown() is declared as constant. No races. */
  if(!tls_socket_test_flag(socket, tls_onreadclose_once)) {
    if(SSL_get_shutdown(socket->ssl) == (SSL_SENT_SHUTDOWN | SSL_RECEIVED_SHUTDOWN)) {
      tls_socket_set_flag(socket, tls_onreadclose_once);
      if(socket->tls_settings->force_close_tcp) {
        tls_socket_force_close(socket);
      } else {
        tcp_socket_close((struct tcp_socket*) socket);
      }
    } else if(SSL_get_shutdown(socket->ssl) == SSL_RECEIVED_SHUTDOWN) {
      tls_socket_set_flag(socket, tls_onreadclose_once);
      switch(socket->tls_settings->onreadclose_auto_res) {
        case tls_onreadclose_callback: {
          socket->tls_callbacks->tls_onreadclose(socket);
          break;
        }
        case tls_onreadclose_tls_close: {
          tls_socket_close(socket);
          break;
        }
        case tls_onreadclose_tcp_close: {
          tcp_socket_close((struct tcp_socket*) socket);
          break;
        }
        case tls_onreadclose_tcp_force_close: {
          tls_socket_force_close(socket);
          break;
        }
      }
    }
  }
}

void tls_onreadclose(struct tcp_socket* soc) {
  if(socket->tls_callbacks->tcp_onreadclose != NULL) {
    socket->tls_callbacks->tcp_onreadclose(socket);
  }
}

void tls_onsend(struct tcp_socket* soc) {
  if(tls_socket_test_flag(socket, tls_wants_send)) {
    (void) tls_internal_send(socket, NULL, 0);
  }
  if(SSL_is_init_finished(socket->ssl) && !tls_socket_test_flag(socket, tls_opened)) {
    tls_socket_set_flag(socket, tls_opened);
    if(socket->tls_callbacks->onopen != NULL) {
      socket->tls_callbacks->onopen(socket);
    }
  }
}

int tls_socket_onnomem(struct tcp_socket* soc) {
  return socket->tls_callbacks->onnomem(socket);
}

void tls_onclose(struct tcp_socket* soc) {
  socket->tls_callbacks->onclose(socket);
}

void tls_onfree(struct tcp_socket* soc) {
  if(socket->tls_callbacks->onfree != NULL) {
    socket->tls_callbacks->onfree(socket);
  }
  (void) pthread_mutex_destroy(&socket->read_lock);
  (void) pthread_mutex_destroy(&socket->ssl_lock);
  SSL_free(socket->ssl);
  if(socket->read_buffer != NULL) {
    free(socket->read_buffer);
  }
  if(socket->server != NULL) {
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
    (void) pthread_mutex_destroy(&socket->read_lock);
    return -1;
  }
  socket->ssl = SSL_new(socket->ctx);
  if(socket->ssl == NULL) {
    (void) pthread_mutex_destroy(&socket->read_lock);
    (void) pthread_mutex_destroy(&socket->ssl_lock);
    return -1;
  }
  if(SSL_set_fd(socket->ssl, socket->base.sfd) == 0) {
    (void) pthread_mutex_destroy(&socket->read_lock);
    (void) pthread_mutex_destroy(&socket->ssl_lock);
    SSL_free(socket->ssl);
    return -1;
  }
  if(which == net_socket) {
    SSL_set_connect_state(socket->ssl);
  } else {
    SSL_set_accept_state(socket->ssl);
  }
  /* Required to shrink and grow the buffer of records to be sent */
  SSL_set_mode(socket->ssl, SSL_MODE_ACCEPT_MOVING_WRITE_BUFFER);
  return 0;
}

int tls_create_socket(struct tls_socket* const socket) {
  return tcp_create_socket((struct tcp_socket*) socket);
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
        if(socket->tls_settings->force_close_on_fatal_error == 1) {
          tls_socket_force_close(socket);
        } else {
          /* We MUST NOT use tls_socket_close(), according to the OpenSSL documentation */
          tcp_socket_close((struct tcp_socket*) socket);
        }
        return -1;
      }
    }
  }
}

/* tls_send returns amount of bytes transfered. If the value is 0, the application
might want to check errno to see if there was any error.
If errno was ENOMEM, after taking action to release some memory, the application
MUST repeat the function *WITH THE SAME ARGUMENTS*. SAME meaning literally the same.
The application probably doesn't need to check for any other errno value other than
ENOMEM. */

int tls_send(struct tls_socket* const socket, const void* data, int size) {
  if(tls_socket_test_flag(socket, tcp_shutdown_wr)) {
    errno = EPIPE;
    return 0;
  }
  (void) pthread_mutex_lock(&socket->lock);
  /* Send buffered records first, if possible. This way, there will be no dumb
  recordering if we first try to send the requested data and epoll has not yet
  woken up to notice it can send the buffered records. */
  int addon = sizeof(struct tls_record);
#define record ((struct tls_record*) socket->send_buffer)
  for(unsigned i = 0; i < socket->send_used; ++i) {
    switch(tls_internal_send(socket, socket->send_buffer + addon, record->size)) {
      case 1: {
        addon += record->total_size;
        break;
      }
      case 0: {
        goto breakout;
      }
      case -1: {
        (void) pthread_mutex_unlock(&socket->lock);
        return 0;
      }
    }
  }
#undef record
  breakout:
  addon -= sizeof(struct tls_record);
  socket->send_used -= addon;
  if(socket->send_used != 0) {
    (void) memmove(socket->send_buffer, socket->send_buffer + addon, socket->send_used);
    if(socket->send_size - socket->send_used >= socket->settings->send_buffer_cleanup_threshold) {
      char* const ptr = realloc(socket->send_buffer, socket->send_used);
      /* Yes, we could call onnomem() here, but then, when do we stop calling it
      if we keep getting OOM? Shrinking the buffer isn't anything that we require
      anyway, so we will be better off not calling onnomem() here. */
      if(ptr != NULL) {
        socket->send_buffer = ptr;
        socket->send_size = socket->send_used;
      }
    }
  } else {
    free(socket->send_buffer);
    socket->send_buffer = NULL;
    socket->send_size = 0;
  }
  (void) pthread_mutex_unlock(&socket->lock);
  /* After we've dealt with the buffered records and we can send more, try sending
  requested data. If can't do it, create and buffer a new record. */
  switch(tls_internal_send(socket, data, size)) {
    case 0: {
      /* Buffer the data */
      (void) pthread_mutex_lock(&socket->lock);
      const unsigned total_size = (size + sizeof(struct tls_record) - 1) & -sizeof(struct tls_record);
      if(socket->send_size - socket->send_used < total_size) {
        while(1) {
          char* const ptr = realloc(socket->send_buffer, socket->send_used + total_size);
          if(ptr != NULL) {
            socket->send_buffer = ptr;
            socket->send_size = socket->send_used + total_size;
            break;
          } else if(socket->tls_callbacks->onnomem(socket) == 0) {
            continue;
          } else {
            (void) pthread_mutex_unlock(&socket->lock);
            errno = ENOMEM;
            return 0;
          }
        }
      }
      *((struct tls_record*)(socket->send_buffer + socket->send_used)) = (struct tls_record) {
        .size = size,
        .total_size = total_size
      };
      (void) memcpy(socket->send_buffer + socket->send_used + sizeof(struct tls_record), data, size);
      socket->send_used += total_size;
      (void) pthread_mutex_unlock(&socket->lock);
    }
    case 1: {
      /* We were able to send the record - best case scenario */
      errno = 0;
      return size;
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
    if(socket->read_used + socket->tls_settings->read_buffer_growth + addon > socket->read_size) {
      while(1) {
        char* const ptr = realloc(socket->read_buffer, socket->read_size + socket->tls_settings->read_buffer_growth);
        /* Here we can't ignore OOM. Either free memory or kill the socket. We
        MUST expand the read buffer, because else we wouldn't be able to process
        the incoming message, whatever it would be. There is no delaying - if we
        can't do it now, we won't get an EPOLLIN again, since this is edge
        triggered. Thus, we can't afford dropping any messages or such. */
        if(ptr != NULL) {
          socket->read_buffer = ptr;
          socket->read_size += socket->tls_settings->read_buffer_growth;
          break;
        } else if(socket->tls_callbacks->onnomem(socket) == 0) {
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
    const int err = SSL_get_error(socket->ssl, SSL_read_ex(socket->ssl, socket->read_buffer + addon, socket->tls_settings->read_buffer_growth, &read));
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
        if(socket->tls_settings->force_close_on_fatal_error == 1) {
          tls_socket_force_close(socket);
        } else {
          /* We MUST NOT use tls_socket_close(), according to the OpenSSL documentation */
          tcp_socket_close((struct tcp_socket*) socket);
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
    if(socket->read_size - socket->read_used >= socket->tls_settings->read_buffer_cleanup_threshold) {
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
contention doesn't become a problem when calling tls_peak_once() repetively, or
when locking the mutex manually and then calling tls_peak() for greater speed.
The underlying code might want to write some bytes to the read buffer at any
time, so well-written applications will probably have better performance by not
creating many lock contention opportunities and using tls_read() to copy memory. */

unsigned char tls_peak(const struct tls_socket* const socket, const int offset) {
  return socket->read_buffer[offset];
}

unsigned char tls_peak_once(struct tls_socket* const socket, const int offset) {
  (void) pthread_mutex_lock(&socket->read_lock);
  const unsigned char byte = tls_peak(socket, offset);
  (void) pthread_mutex_unlock(&socket->read_lock);
  return byte;
}



#define socket ((struct tls_socket*) sock)

int tls_onconnection(struct tcp_socket* sock) {
  socket->ctx = socket->server->ctx;
  return socket->server->tls_callbacks->onconnection(socket);
}

#undef socket

#define server ((struct tls_server*) serv)

int tls_server_onnomem(struct tcp_server* serv) {
  return server->tls_callbacks->onnomem(server);
}

void tls_onshutdown(struct tcp_server* serv) {
  server->tls_callbacks->onshutdown(server);
}

#undef server



void tls_server_free(struct tls_server* const server) {
  tcp_server_free((struct tcp_server*) server);
}

int tls_create_server(struct tls_server* const server) {
  if(server->settings->socket_size == 0) {
    server->settings->socket_size = sizeof(struct tls_socket);
  }
  return tcp_create_server((struct tcp_server*) server);
}

void tls_server_foreach_conn(struct tls_server* const server, void (*callback)(struct tls_socket*, void*), void* data, const int write) {
  tcp_server_foreach_conn((struct tcp_server*) server, (void (*)(struct tcp_socket*,void*)) callback, data, write);
}

void tls_server_dont_accept_conn(struct tls_server* const server) {
  tcp_server_dont_accept_conn((struct tcp_server*) server);
}

void tls_server_accept_conn(struct tls_server* const server) {
  tcp_server_accept_conn((struct tcp_server*) server);
}

int tls_server_shutdown(struct tls_server* const server) {
  return tcp_server_shutdown((struct tcp_server*) server);
}

unsigned tls_server_get_conn_amount_raw(const struct tls_server* const server) {
  return tcp_server_get_conn_amount_raw((struct tcp_server*) server);
}

unsigned tls_server_get_conn_amount(struct tls_server* const server) {
  return tcp_server_get_conn_amount((struct tcp_server*) server);
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