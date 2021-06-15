#include "tcp.h"
#include "debug.h"
#include <assert.h>

#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

#include <openssl/ssl.h>
#include <openssl/bio.h>
#include <openssl/err.h>

static void tcp_socket_set_state(struct tcp_socket* const socket, const unsigned new_state) {
  atomic_store(&socket->state, (atomic_load(&socket->state) & (~TCP_INTERNAL_STATE_BIT_SIZE)) | new_state);
}

static unsigned tcp_socket_get_state(const struct tcp_socket* const socket) {
  return atomic_load(&socket->state) & TCP_INTERNAL_STATE_BIT_SIZE;
}



static void tcp_socket_set_flags(struct tcp_socket* const socket, const unsigned new_flags) {
  atomic_store(&socket->state, (atomic_load(&socket->state) & TCP_INTERNAL_STATE_BIT_SIZE) | (new_flags << TCP_INTERNAL_STATE_BITSHIFT));
}

static void tcp_socket_set_flag(struct tcp_socket* const socket, const unsigned flag) {
  atomic_store(&socket->state, atomic_load(&socket->state) | (flag << TCP_INTERNAL_STATE_BITSHIFT));
}

static unsigned tcp_socket_get_flags(const struct tcp_socket* const socket) {
  return atomic_load(&socket->state) >> TCP_INTERNAL_STATE_BITSHIFT;
}

static unsigned tcp_socket_test_flag(const struct tcp_socket* const socket, const unsigned flag) {
  return tcp_socket_get_flags(socket) & flag;
}

static void tcp_socket_clear_flags(struct tcp_socket* const socket) {
  atomic_store(&socket->state, atomic_load(&socket->state) & TCP_INTERNAL_STATE_BIT_SIZE);
}

static void tcp_socket_clear_flag(struct tcp_socket* const socket, const unsigned flag) {
  atomic_store(&socket->state, (tcp_socket_get_flags(socket) & (~flag)) << TCP_INTERNAL_STATE_BITSHIFT);
}



static unsigned tcp_socket_can_close(const struct tcp_socket* const socket) {
  return tcp_socket_test_flag(socket, tcp_can_close);
}

static int tcp_socket_can_proceed(const struct tcp_socket* const socket) {
  const int state = tcp_socket_get_state(socket);
  if(state != tcp_connected && state != tcp_closing) {
    return net_failure;
  }
  return net_success;
}

static int tcp_socket_can_proceed_send(const struct tcp_socket* const socket) {
  const int state = tcp_socket_get_state(socket);
  if(state != tcp_connected && (state != tcp_closing || tcp_socket_test_flag(socket, tcp_internal_shutdown_wr))) {
    return net_failure;
  }
  return net_success;
}

static int tcp_socket_can_proceed_read(const struct tcp_socket* const socket) {
  const int state = tcp_socket_get_state(socket);
  if(state != tcp_connected && (state != tcp_closing || tcp_socket_test_flag(socket, tcp_internal_shutdown_rd))) {
    return net_failure;
  }
  return net_success;
}

static void tcp_socket_set_close_reason(struct tcp_socket* const socket, const int reason) {
  socket->close_reason = reason;
}



static void tcp_socket_free(struct tcp_socket* const socket) {
  printf_debug("tcp_socket_free() socket sfd %d", 0, socket->base.sfd);
  assert(socket->base.sfd > 0);
  
  SSL_free(socket->ssl);
  (void) pthread_rwlock_destroy(&socket->lock);
  free(socket->send_buffer);
  (void) close(socket->base.sfd);
  socket->base.sfd = -1;
  const unsigned offset = sizeof(struct net_socket_base) + 4 * sizeof(long) + sizeof(int);
  (void) memset((char*) socket + offset, 0, sizeof(struct tcp_socket) - offset);
}

static void tcp_socket_close_close(struct tcp_socket* const socket) {
  printf_debug("tcp_socket_close_close() socket sfd %d", 0, socket->base.sfd);
  assert(socket->base.sfd > 0);
  
  /* Could either waste time locking the rwlock, or set the state first */
  tcp_socket_set_state(socket, tcp_closed);
  (void) close(socket->base.sfd);
}

int tcp_is_fatal(const int err) {
  printf_debug("tcp_is_fatal() err %d", 0, err);
  switch(err) {
    case ETIMEDOUT:
    case ENETUNREACH:
    case ECONNRESET:
    case EHOSTUNREACH:
    case ECONNREFUSED:
    case ENETDOWN: return tcp_fatal;
    default: return tcp_proceed;
  }
}

void tcp_socket_close(struct tcp_socket* const socket) {
  printf_debug("tcp_socket_close() socket sfd %d", 0, socket->base.sfd);
  assert(socket->base.sfd > 0);
  
  if(socket->ctx == NULL) {
    if(tcp_socket_get_state(socket) != tcp_closed) {
      tcp_socket_set_flag(socket, tcp_internal_shutdown_wr);
      tcp_socket_set_state(socket, tcp_closing);
    }
    (void) shutdown(socket->base.sfd, SHUT_WR);
  } else {
    assert(0);
    // SSLTODO: SSL_shutdown()
  }
}

/* It is safe to call tcp_socket_confirm_close() during the lifetime of the
socket and keep accessing it afterwards, until the onclose() handler gets called.
After it's called it is PROHIBITED to use the socket after using tcp_socket_confirm_close().
ALWAYS use tcp_socket_confirm_close() after a fatal error (ECONNRESET, etc).*/

void tcp_socket_confirm_close(struct tcp_socket* const socket) {
  assert(socket->base.sfd > 0);
  printf_debug("tcp_socket_confirm_close() socket sfd %d can close %d state %d", 0, socket->base.sfd, tcp_socket_can_close(socket) != 0, tcp_socket_get_state(socket));
  
  if(socket->base.sfd == -1 || tcp_socket_can_close(socket)) {
    return;
  }
  tcp_socket_set_flag(socket, tcp_can_close);
  switch(tcp_socket_get_state(socket)) {
    case tcp_connecting: {
      /* We aren't allowed to call any functions yet, so we can simply destroy */
      tcp_socket_free(socket);
      break;
    }
    case tcp_connected: {
      /* Send FIN to the peer, let epoll handle the rest */
      tcp_socket_close(socket);
      break;
    }
    case tcp_closing: {
      /* Wait until both ways of the connection are closed. Epoll will deal with that */
      break;
    }
    case tcp_closed: {
      /* Epoll has dealt with the socket, now we can simply free it */
      tcp_socket_free(socket);
      break;
    }
  }
}

void tcp_socket_confirm_force_close(struct tcp_socket* const socket) {
  printf_debug("tcp_socket_confirm_force_close() socket sfd %d", 0, socket->base.sfd);
  assert(socket->base.sfd > 0);
  
  tcp_socket_set_flag(socket, tcp_can_close);
  tcp_socket_close_close(socket);
}

static int tcp_socket_tls_handle_error(struct tcp_socket* const socket, const int ret_value) {
  switch(SSL_get_error(socket->ssl, ret_value)) {
    case SSL_ERROR_NONE: {
      printf_debug("tcp_socket_tls_handle_error() socket sfd %d ret val %d SSL_ERROR_NONE", 1, socket->base.sfd, ret_value);
      return tcp_proceed;
    }
    case SSL_ERROR_ZERO_RETURN: {
      printf_debug("tcp_socket_tls_handle_error() socket sfd %d ret val %d SSL_ERROR_ZERO_RETURN", 1, socket->base.sfd, ret_value);
      /* tcp_read() will handle this for us. */
      return tcp_fatal;
    }
    case SSL_ERROR_WANT_READ: {
      printf_debug("tcp_socket_tls_handle_error() socket sfd %d ret val %d SSL_ERROR_WANT_READ", 1, socket->base.sfd, ret_value);
      /* We won't let the application automatically read data, because it might
      not actually do it afterall. To not disrupt TLS, we will handle it ourselves. */
      tcp_socket_set_flag(socket, tcp_wants_read);
      return tcp_proceed;
    }
    case SSL_ERROR_WANT_WRITE: {
      printf_debug("tcp_socket_tls_handle_error() socket sfd %d ret val %d SSL_ERROR_WANT_WRITE", 1, socket->base.sfd, ret_value);
      /* This requires special care, because we only call SSL_write() and nothing else (I think) */
      tcp_socket_set_flag(socket, tcp_wants_send);
      return tcp_proceed;
    }
    case SSL_ERROR_SYSCALL: {
      printf_debug("tcp_socket_tls_handle_error() socket sfd %d ret val %d SSL_ERROR_SYSCALL", 1, socket->base.sfd, ret_value);
      /* Not recoverable - prohibited to call any SSL function */
      tcp_socket_set_state(socket, tcp_closed);
      tcp_socket_close(socket);
      return tcp_fatal;
    }
    case SSL_ERROR_SSL: {
      printf_debug("tcp_socket_tls_handle_error() socket sfd %d ret val %d SSL_ERROR_SSL", 1, socket->base.sfd, ret_value);
      /* Not recoverable - prohibited to call any SSL function */
      tcp_socket_set_state(socket, tcp_closed);
      tcp_socket_close(socket);
      return tcp_fatal;
    }
    default: return tcp_fatal;
  }
}




int tcp_create_socket_base(struct tcp_socket* const sock) {
  const int sfd = socket(net_get_family(&sock->base.addr), stream_socktype, tcp_protocol);
  if(sfd == -1) {
    return net_failure;
  }
  sock->base.sfd = sfd;
  return net_success;
}

int tcp_socket_tls(struct tcp_socket* const socket) {
  socket->read_bio = BIO_new(BIO_s_mem());
  if(socket->read_bio == NULL) {
    return net_out_of_memory;
  }
  socket->send_bio = BIO_new(BIO_s_mem());
  if(socket->send_bio == NULL) {
    (void) BIO_free(socket->read_bio);
    return net_out_of_memory;
  }
  socket->ssl = SSL_new(socket->ctx);
  if(socket->ssl == NULL) {
    (void) BIO_free(socket->read_bio);
    (void) BIO_free(socket->send_bio);
    return net_out_of_memory;
  }
  (void) BIO_set_nbio(socket->read_bio, 1);
  (void) BIO_set_nbio(socket->send_bio, 1);
  SSL_set_connect_state(socket->ssl);
  SSL_set_bio(socket->ssl, socket->read_bio, socket->send_bio);
  return net_success;
}

int tcp_create_socket(struct tcp_socket* const socket) {
  printf_debug("tcp_create_socket()", 0);
  int ret = pthread_rwlock_init(&socket->lock, NULL);
  if(ret != 0) {
    errno = ret;
    return net_failure;
  }
  printf_debug("tcp_create_socket() pthread_rwlock_init()", 0);
  if(tcp_create_socket_base(socket) == net_failure) {
    tcp_socket_free(socket);
    return net_failure;
  }
  socket->which = tcp_socket;
  printf_debug("tcp_create_socket() tcp_create_socket_base()", 0);
  if(net_socket_base_options(socket->base.sfd) == net_failure) {
    tcp_socket_free(socket);
    return net_failure;
  }
  printf_debug("tcp_create_socket() net_socket_base_options()", 0);
  if(socket->ctx != NULL && tcp_socket_tls(socket) != net_success) {
    tcp_socket_free(socket);
    errno = ENOMEM;
    return net_failure;
  }
  printf_debug("tcp_create_socket() tcp_socket_tls()", 0);
  tcp_socket_set_state(socket, tcp_connecting);
  ret = net_connect_socket(socket->base.sfd, &socket->base.addr);
  if(ret == -1 && errno != EINPROGRESS) {
    tcp_socket_confirm_close(socket);
    return net_failure;
  }
  printf_debug("tcp_create_socket() net_connect_socket()", 0);
  socket->base.events = EPOLLET | EPOLLRDHUP | EPOLLIN | EPOLLOUT;
  if(net_epoll_add(socket->epoll, &socket->base) == net_failure) {
    tcp_socket_free(socket);
    return net_failure;
  }
  printf_debug("tcp_create_socket() net_epoll_add()", 0);
  return net_success;
}

/*
tcp_send() returns amount of bytes written. If the amount is less than input,
an out of memory error has occured. If the amount is -1, an error has occured.
If the amount is -2, a fatal error has occured and the socket must be destroyed.
*/

static int tcp_internal_send(struct tcp_socket* const socket, const void* data, int size, const int lock) {
  printf_debug("tcp_internal_send() socket sfd %d", 0, socket->base.sfd);
  assert(socket->base.sfd > 0);
  
  if(lock == 0) {
    if(tcp_socket_can_proceed_send(socket) == net_failure) {
      printf_debug("tcp_internal_send() can't proceed 1", 0);
      errno = ENOTCONN;
      return -1;
    }
    (void) pthread_rwlock_wrlock(&socket->lock);
    if(tcp_socket_can_proceed_send(socket) == net_failure) {
      printf_debug("tcp_internal_send() can't proceed 2", 0);
      errno = ENOTCONN;
      goto error;
    }
  }
  printf_debug("tcp_internal_send() tcp_can_send is %d", 0, tcp_socket_test_flag(socket, tcp_can_send) != 0);
  if(socket->ctx != NULL) {
    assert(0);
    /* First check if the BIO can store some or all of our data */
    while(1) {
      const int written = BIO_write(socket->send_bio, data, size);
      if(written >= 0) {
        size -= written;
        socket->send_total_size += written;
        if(size == 0) {
          /* Wrote everything, best case scenario */
          if(lock == 0) {
            (void) pthread_rwlock_unlock(&socket->lock);
          }
          return written;
        }
        data = (char*) data + written;
      } else if(BIO_should_retry(socket->send_bio) == 0) {
        /* A fatal error, destroy the socket */
        if(lock == 0) {
          (void) pthread_rwlock_unlock(&socket->lock);
        }
        tcp_socket_close(socket);
        goto fatal_error;
      }
      /* We can retry, but later */
      break;
    }
    // SSLTODO: check if we can send and if we can, do SSL_write()
  } else if(tcp_socket_test_flag(socket, tcp_can_send)) {
    /* We are allowed to send data. Do it instead of waiting for epoll to do so
    to preserve memory. */
    const int all = size;
    while(1) {
      const ssize_t bytes = send(socket->base.sfd, data, size, MSG_NOSIGNAL);
      printf_debug("tcp_internal_send() send() yielded %ld bytes, errno %d", 0, bytes, errno);
      if(bytes == -1) {
        switch(errno) {
          case EINTR: continue;
          case ENOBUFS: break;
          case EAGAIN: {
            tcp_socket_clear_flag(socket, tcp_can_send);
            break;
          }
          case EPIPE: {
            /* We cannot send any more data. Let epoll handle this */
            tcp_socket_clear_flag(socket, tcp_can_send);
            goto error;
          }
          default: {
            if(tcp_is_fatal(errno)) {
              tcp_socket_set_state(socket, tcp_closed);
              tcp_socket_set_close_reason(socket, errno);
              goto fatal_error;
            }
            goto error;
          }
        }
      }
      size -= bytes;
      if(size == 0) {
        /* Wrote everything, best case scenario */
        if(lock == 0) {
          (void) pthread_rwlock_unlock(&socket->lock);
        }
        return all;
      }
      data = (char*) data + bytes;
    }
  }
  /* If not everything was processed, store it for later */
  if(socket->send_used + size > socket->send_size) {
    /* Try to fit everything into the buffer by resizing it */
    char* const ptr = realloc(socket->send_buffer, socket->send_used + size);
    if(ptr == NULL) {
      size = socket->send_size - socket->send_used;
      /* If resize failed, but there is some free space (size != 0), use it */
      (void) memcpy(socket->send_buffer + socket->send_used, data, size);
      socket->send_used += size;
      if(lock == 0) {
        (void) pthread_rwlock_unlock(&socket->lock);
      }
      return size;
    }
    socket->send_buffer = ptr;
    socket->send_size += size;
  }
  /* Append the data */
  (void) memcpy(socket->send_buffer + socket->send_used, data, size);
  socket->send_used += size;
  if(lock == 0) {
    (void) pthread_rwlock_unlock(&socket->lock);
  }
  return size;
  error:
  if(lock == 0) {
    (void) pthread_rwlock_unlock(&socket->lock);
  }
  return -1;
  fatal_error:
  if(lock == 0) {
    (void) pthread_rwlock_unlock(&socket->lock);
  }
  return -2;
}

int tcp_send(struct tcp_socket* const socket, const void* data, int size) {
  return tcp_internal_send(socket, data, size, 0);
}

int tcp_handler_send(struct tcp_socket* const socket, const void* data, int size) {
  return tcp_internal_send(socket, data, size, 1);
}

/* tcp_read() returns amount of bytes read. If the amount is -1, an error has occured.
If the amount is -2, a fatal error has occured and the socket must be destroyed. If
the amount is 0, the peer won't send more messages and the application must call
tcp_socket_close() after it is finished sending data. */

static int tcp_internal_read(struct tcp_socket* const socket, void* data, int size, const int lock, int* const read_amount) {
  printf_debug("tcp_internal_read() socket sfd %d", 0, socket->base.sfd);
  assert(socket->base.sfd > 0);
  
  if(lock == 0) {
    if(tcp_socket_can_proceed_read(socket) == net_failure) {
      printf_debug("tcp_internal_read() can't proceed 1", 0);
      errno = ENOTCONN;
      return -1;
    }
    (void) pthread_rwlock_wrlock(&socket->lock);
    if(tcp_socket_can_proceed_read(socket) == net_failure) {
      printf_debug("tcp_internal_read() can't proceed 2", 0);
      errno = ENOTCONN;
      (void) pthread_rwlock_unlock(&socket->lock);
      return -1;
    }
  }
  printf_debug("tcp_internal_read() tcp_can_read is %d", 0, tcp_socket_test_flag(socket, tcp_can_read) != 0);
  if(tcp_socket_test_flag(socket, tcp_can_read)) {
    int read = 0;
    while(1) {
      const ssize_t bytes = recv(socket->base.sfd, data, size, 0);
      printf_debug("tcp_internal_read() recv() yielded %ld bytes, errno %d", 0, bytes, errno);
      if(bytes == -1) {
        switch(errno) {
          case EINTR: continue;
          case EAGAIN: {
            tcp_socket_clear_flag(socket, tcp_can_read);
            errno = 0;
            break;
          }
          default: {
            if(tcp_is_fatal(errno)) {
              tcp_socket_set_state(socket, tcp_closed);
              tcp_socket_set_close_reason(socket, errno);
              if(lock == 0) {
                (void) pthread_rwlock_unlock(&socket->lock);
              }
              return -2;
            }
            if(lock == 0) {
              (void) pthread_rwlock_unlock(&socket->lock);
            }
            return -1;
          }
        }
      } else if(bytes == 0) {
        /* End of data, let epoll deal with the socket */
        if(lock == 0) {
          (void) pthread_rwlock_unlock(&socket->lock);
        }
        *read_amount = read;
        errno = EPIPE;
        return 0;
      }
      read += bytes;
      if(read == size) {
        /* Read everything, best case scenario */
        if(lock == 0) {
          (void) pthread_rwlock_unlock(&socket->lock);
        }
        *read_amount = size;
        return 1;
      }
      /* If we read less than requested, we either have no more data in the
      kernel buffer, or a FIN has arrived. Either way, we don't need to adjust
      data and size, just retry the system call in the next iteration. */
    }
  }
  return 0; /* To not generate a compiler warning */
}

int tcp_read(struct tcp_socket* const socket, void* const data, const int size, int* const read_amount) {
  return tcp_internal_read(socket, data, size, 0, read_amount);
}

int tcp_handler_read(struct tcp_socket* const socket, void* const data, const int size, int* const read_amount) {
  return tcp_internal_read(socket, data, size, 1, read_amount);
}

/* SSLTODO: write tcp_read_ssl():

1. tcp_read with a buffer size 4096 (BIO size),

2. BIO_write(socket->read_bio),

3. SSL_read(socket->ssl),

4. repeat step 1 until EAGAIN (or increase 4096)
*/

static int tcp_internal_read_ssl(struct tcp_socket* const socket, void* data, int size, const int lock, int* const read_amount) {
  printf_debug("tcp_internal_read_ssl() socket sfd %d", 0, socket->base.sfd);
  assert(socket->base.sfd > 0);
  
  if(lock == 0) {
    if(tcp_socket_can_proceed_read(socket) == net_failure) {
      printf_debug("tcp_internal_read_ssl() can't proceed 1", 0);
      errno = ENOTCONN;
      return -1;
    }
    (void) pthread_rwlock_wrlock(&socket->lock);
    if(tcp_socket_can_proceed_read(socket) == net_failure) {
      printf_debug("tcp_internal_read_ssl() can't proceed 2", 0);
      errno = ENOTCONN;
      (void) pthread_rwlock_unlock(&socket->lock);
      return -1;
    }
  }
  printf_debug("tcp_internal_read_ssl() tcp_can_read is %d", 0, tcp_socket_test_flag(socket, tcp_can_read) != 0);
  
  char buffer[4096];
  int read;
  const int status = tcp_handler_read(socket, buffer, 4096, &read); // don't read too much
  // IMO we CANNOT store excess data in a buffer, bc that defeats TCP's congestion control
  // OR we can have a buffer up to 4096 bytes
  if(read == 0) {
    return status;
  }
  int written = 
  
  return status;
}

int tcp_read_ssl(struct tcp_socket* const socket, void* const data, const int size, int* const read_amount) {
  return tcp_internal_read_ssl(socket, data, size, 0, read_amount);
}

int tcp_handler_read_ssl(struct tcp_socket* const socket, void* const data, const int size, int* const read_amount) {
  return tcp_internal_read_ssl(socket, data, size, 1, read_amount);
}

#define socket ((struct tcp_socket*) structure)

#include <unistd.h>

static void tcp_socket_onevent(struct net_epoll* epoll, int events, void* structure) {
  (void) pthread_rwlock_wrlock(&socket->lock);
  if(events & EPOLLERR) {
    printf_debug("tcp_socket_onevent() EPOLLERR socket sfd %d", 0, socket->base.sfd);
    /* Check any errors we could have got from connect() or such */
    int code;
    if(getsockopt(socket->base.sfd, tcp_protocol, SO_ERROR, &code, &(socklen_t){sizeof(int)}) == 0) {
      if(tcp_is_fatal(code)) {
        tcp_socket_set_state(socket, tcp_closed);
        tcp_socket_set_close_reason(socket, errno);
      } else if(socket->callbacks->onerror(socket) == tcp_fatal) {
        (void) pthread_rwlock_unlock(&socket->lock);
        return;
      }
    }
  }
  if(events & EPOLLRDHUP) {
    printf_debug("tcp_socket_onevent() EPOLLRDHUP socket sfd %d", 0, socket->base.sfd);
    tcp_socket_clear_flag(socket, tcp_can_read);
    /* We need to send back a FIN, but let the application choose the
    right moment. For now only mark the socket as closing. */
    tcp_socket_set_state(socket, tcp_closing);
    tcp_socket_set_flag(socket, tcp_internal_shutdown_rd);
  }
  if(events & EPOLLHUP) {
    printf_debug("tcp_socket_onevent() EPOLLHUP socket sfd %d", 0, socket->base.sfd);
    /* The connection is closed. Warning: we CANNOT set socket as closed here,
    because the application could call tcp_socket_confirm_close() after we set
    it but before we call onclose(), causing it to free the socket, but since
    onclose() didn't happen, it will think it's still able to use it. Note that
    it doesn't matter what state the socket is right now - even if the application
    does call tcp_socket_confirm_close() at this moment:
    
    - When we are tcp_connected, it will call shutdown(socket, SHUT_WR), which
    will ultimately fail with ENOTCONN and do nothing,
    
    - When we are tcp_closing, it will do nothing
    */
    (void) pthread_rwlock_unlock(&socket->lock);
    socket->callbacks->onclose(socket); // TODO: write 2 versions of functions - locking the rwlock and ones that run without it inside event handlers
    /* Now we set it as closed, because onclose() happened */
    tcp_socket_set_state(socket, tcp_closed);
    /* In case the application already confirmed that the socket can be freed,
    we check for it. Otherwise it will never be freed. */
    if(tcp_socket_can_close(socket)) {
      tcp_socket_free(socket);
    }
    return;
  }
  if((events & EPOLLOUT) || tcp_socket_test_flag(socket, tcp_can_send)) {
    printf_debug("tcp_socket_onevent() EPOLLOUT socket sfd %d", 0, socket->base.sfd);
    if(events & EPOLLOUT) {
      tcp_socket_set_flag(socket, tcp_can_send);
    }
    if(tcp_socket_get_state(socket) == tcp_connecting) {
      /* connect() succeeded */
      if(socket->ctx == NULL) {
        tcp_socket_set_state(socket, tcp_connected);
        socket->callbacks->onopen(socket);
      } else {
        switch(SSL_do_handshake(socket->ssl)) {
          case 1: {
            printf_debug("tcp_socket_onevent() SSL_do_handshake() yielded 1 socket sfd %d", 1, socket->base.sfd);
            tcp_socket_set_state(socket, tcp_connected);
            socket->callbacks->onopen(socket);
            break;
          }
          case 0: {
            printf_debug("tcp_socket_onevent() SSL_do_handshake() yielded 0 socket sfd %d", 1, socket->base.sfd);
            if(tcp_socket_tls_handle_error(socket, 0) == tcp_fatal) {
              return;
            }
            break;
          }
          case -1: {
            printf_debug("tcp_socket_onevent() SSL_do_handshake() yielded -1 socket sfd %d", 1, socket->base.sfd);
            if(tcp_socket_tls_handle_error(socket, -1) == tcp_fatal) {
              return;
            }
            break;
          }
        }
      }
    } /* There is no "else" statement, because the application could have
    scheduled some data to be sent in the onopen() function */
    if(socket->ctx != NULL) {
      if(tcp_socket_test_flag(socket, tcp_wants_send)) {
        printf_debug("tcp_socket_onevent() tcp_wants_send socket sfd %d", 1, socket->base.sfd);
        size_t written;
        const int status = SSL_write_ex(socket->ssl, NULL, 0, &written);
        if(status <= 0 && tcp_socket_tls_handle_error(socket, status) == tcp_fatal) {
          printf_debug("tcp_socket_onevent() SSL_write_ex() fatal error socket sfd %d", 1, socket->base.sfd);
          return;
        }
        printf_debug("tcp_socket_onevent() SSL_write_ex() socket sfd %d", 1, socket->base.sfd);
      } else if(socket->send_total_size > 0) { // SSLTODO: do BIO_write with what we have (send_used), then SSL_write(), shift data, and thats it
        
      }
    } else if(socket->send_used > 0) {
      /* Simply send contents of our send_buffer in FIFO manner */
      int addon = 0;
      while(1) {
        const ssize_t bytes = send(socket->base.sfd, socket->send_buffer + addon, socket->send_used, MSG_NOSIGNAL);
        if(bytes == -1) {
          if(errno == EINTR) {
            continue;
          } else {
            /* Wait until the next epoll iteration. If the error was fatal, the connection
            will be closed with an EPOLLHUP. Otherwise we can probably ignore it. */
            if(errno == EAGAIN) {
              /* If kernel doesn't accept more data, don't send more in the future */
              tcp_socket_clear_flag(socket, tcp_can_send);
            }
            break;
          }
        }
        socket->send_used -= bytes;
        if(socket->send_used == 0 && socket->settings->send_buffer_allow_freeing == 1) {
          free(socket->send_buffer);
          socket->send_buffer = NULL;
          socket->send_size = 0;
          break;
        }
        addon += bytes;
      }
      /* Shift the data we didn't send, maybe resize the buffer */
      if(socket->send_used != 0) {
        (void) memmove(socket->send_buffer, socket->send_buffer + addon, socket->send_used);
        if(socket->send_size - socket->send_used >= socket->settings->send_buffer_cleanup_threshold) {
          while(1) {
            char* const ptr = realloc(socket->send_buffer, socket->send_size - socket->settings->send_buffer_cleanup_threshold);
            if(ptr == NULL) {
              if(socket->callbacks->onerror(socket) == tcp_proceed) {
                continue;
              } else {
                (void) pthread_rwlock_unlock(&socket->lock);
                return;
              }
            }
            socket->send_buffer = ptr;
            socket->send_size -= socket->settings->send_buffer_cleanup_threshold;
            break;
          }
        }
      }
    }
  }
  if((events & EPOLLIN) || tcp_socket_test_flag(socket, tcp_can_read)) {
    printf_debug("tcp_socket_onevent() EPOLLIN socket sfd %d", 0, socket->base.sfd);
    if(events & EPOLLIN) {
      tcp_socket_set_flag(socket, tcp_can_read);
    }
    if(socket->ctx != NULL) {
      assert(0);
      // SSLTODO: first we need to SSL_read, then BIO_read
    } else {
      socket->callbacks->onmessage(socket);
    }
  }
  (void) pthread_rwlock_unlock(&socket->lock);
}

#undef socket



















void tcp_onevent(struct net_epoll* epoll, int events, void* structure) {
  if(((struct tcp_storage*) structure)->which == tcp_socket) {
    tcp_socket_onevent(epoll, events, structure);
  } else {
    assert(0);
    //tcp_server_onevent(epoll, events, structure);
  }
}