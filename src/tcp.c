#include "tcp.h"
#include "aflags.h"

#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <netinet/tcp.h>

/* Race conditions don't matter here. At most, send() or recv() might fail.
Lock contention would add much more overhead than one or two system calls.
All we care about is to not free the socket without application's consent.

One race condition that matters though is multiple epolls operating on a
socket at the same time. We can't allow that and it MUST NOT happen. */

static inline uint32_t tcp_socket_get_state(const struct tcp_socket* const socket) {
  return aflag32_get(&socket->state) & 1;
}

static inline void tcp_socket_set_flag(struct tcp_socket* const socket, const uint32_t flag) {
  aflag32_add(&socket->state, flag);
}

static inline uint32_t tcp_socket_test_flag(const struct tcp_socket* const socket, const uint32_t flag) {
  return aflag32_test(&socket->state, flag);
}

static inline void tcp_socket_clear_flag(struct tcp_socket* const socket, const uint32_t flag) {
  aflag32_del(&socket->state, flag);
}

static inline void tcp_socket_set_state_closed(struct tcp_socket* const socket) {
  tcp_socket_clear_flag(socket, tcp_connected);
}

static inline void tcp_socket_set_state_connected(struct tcp_socket* const socket) {
  tcp_socket_set_flag(socket, tcp_connected);
}



void tcp_socket_cork_on(struct tcp_socket* const socket) {
  tcp_socket_set_flag(socket, tcp_cork);
}

void tcp_socket_cork_off(struct tcp_socket* const socket) {
  tcp_socket_clear_flag(socket, tcp_cork);
}

static int tcp_socket_keepalive_internal(const int sfd, const int retries, const int idle_time, const int reprobe_time) {
  if(net_socket_setopt_true(sfd, SOL_SOCKET, SO_KEEPALIVE) != 0) {
    return net_failure;
  }
  if(setsockopt(sfd, IPPROTO_TCP, TCP_KEEPCNT, &retries, sizeof(int)) != 0) {
    return net_failure;
  }
  if(setsockopt(sfd, IPPROTO_TCP, TCP_KEEPIDLE, &idle_time, sizeof(int)) != 0) {
    return net_failure;
  }
  if(setsockopt(sfd, IPPROTO_TCP, TCP_KEEPINTVL, &reprobe_time, sizeof(int)) != 0) {
    return net_failure;
  }
  return net_success;
}

int tcp_socket_keepalive(const struct tcp_socket* const socket) {
  return tcp_socket_keepalive_internal(socket->base.sfd, 10, 1, 1);
}

int tcp_socket_keepalive_explicit(const struct tcp_socket* const socket, const int retries, const int idle_time, const int reprobe_time) {
  return tcp_socket_keepalive_internal(socket->base.sfd, retries, idle_time, reprobe_time);
}



static void tcp_socket_free(struct tcp_socket* const socket) {
  if(socket->server != NULL) {
    /* By design, the server is either tcp_server_closing, or it is not. It can't
    become closing in this function. That means we don't have to lock it's rwlock.
    Additionally, since the order update in net_epoll, socket deletions will always
    be handled before server shutdowns, so we are actually GUARANTEED here that
    the server isn't closing. That means we can simply ignore the check and don't
    need to check if connections is 0 to call the onshutdown callback. */
    (void) pthread_rwlock_wrlock(&socket->server->lock);
    (void) pthread_mutex_destroy(&socket->lock);
    if(socket->send_buffer != NULL) {
      free(socket->send_buffer);
    }
    (void) close(socket->base.sfd);
    /* Return the socket's index for reuse */
    const unsigned index = ((uintptr_t) socket - (uintptr_t) socket->server->sockets) / sizeof(struct tcp_socket);
    socket->server->freeidx[socket->server->freeidx_used++] = index;
    (void) atomic_fetch_sub(&socket->server->connections, 1);
    struct tcp_server* const server = socket->server;
    (void) memset(socket, 0, sizeof(struct tcp_socket));
    (void) pthread_rwlock_unlock(&server->lock);
  } else {
    (void) pthread_mutex_destroy(&socket->lock);
    if(socket->send_buffer != NULL) {
      free(socket->send_buffer);
    }
    (void) close(socket->base.sfd);
    socket->base.sfd = -1;
    const unsigned offset = offsetof(struct tcp_socket, lock);
    (void) memset((char*) socket + offset, 0, sizeof(struct tcp_socket) - offset);
  }
}

/* Attempt to gracefully close the connection */

void tcp_socket_close(const struct tcp_socket* const socket) {
  (void) shutdown(socket->base.sfd, SHUT_WR);
}

/* This function MUST only be called from within the onclose() callback or after
it occurs, NOT BEFORE. If you want to close a socket while it is still active,
use tcp_socket_close() or tcp_socket_force_close(). */

void tcp_socket_confirm_close(struct tcp_socket* const socket) {
  if(tcp_socket_get_state(socket) == tcp_closed) {
    /* Epoll has dealt with the socket, now we can simply free it */
    tcp_socket_free(socket);
  }
}

/* Send a RST packet to the peer, causing instant disconnection and instant kernel
memory cleanup when it happens. It won't happen instantly though - we need to wait
for the socket's epoll to not do any operations on the socket, which we do using
the wakeup file descriptor and by signaling we want it to destroy this socket.
Delayed termination is caused by how epoll operates - even if we call
close(socket->base.sfd) at any time, there will be no onclose(), since the file
descriptor will be deleted from the epoll automatically by the kernel. And we can't
call onclose() at the end of tcp_socket_force_close(), because then we would imply
freeing the socket, which we MUST NOT do, because epoll might be doing operations
on it in the background, ultimately causing undefined behavior.
Additionally, it isn't guaranteed that onclose() will ever be called, because if
starvation occurs, epoll will never find out about the socket wanting to be closed.
Starvation is a serious problem that is fixed by several eventing libraries, but
it should not happen in the first place. The application should manage sockets in
such a way that does not lead to starvation, by for instance writing efficient code,
benchmarking, increasing event array size. */

int tcp_socket_force_close(struct tcp_socket* const socket) {
  /* If it fails, let it be. It's not a requirement. */
  (void) setsockopt(socket->base.sfd, SOL_SOCKET, SO_LINGER, &((struct linger) { .l_onoff = 1, .l_linger = 0 }), sizeof(struct linger));
  /* Because we don't instantly close() */
  (void) shutdown(socket->base.sfd, SHUT_RDWR);
  return net_epoll_safe_remove(socket->epoll, &socket->base);
}




static void tcp_socket_onclose_internal(struct net_socket_base* base) {
  tcp_socket_set_state_closed((struct tcp_socket*) base);
  ((struct tcp_socket*) base)->callbacks->onclose((struct tcp_socket*) base);
}

int tcp_create_socket_base(struct tcp_socket* const sock) {
  const int sfd = socket(net_get_family(&sock->base.addr), stream_socktype, tcp_protocol);
  if(sfd == -1) {
    return net_failure;
  }
  sock->base.sfd = sfd;
  sock->base.which = net_socket;
  sock->base.events = EPOLLET | EPOLLRDHUP | EPOLLIN | EPOLLOUT;
  sock->base.onclose = tcp_socket_onclose_internal;
  return net_success;
}

int tcp_create_socket(struct tcp_socket* const socket) {
  if(socket->settings->disable_send_buffer == 0) {
    const int ret = pthread_mutex_init(&socket->lock, NULL);
    if(ret != 0) {
      errno = ret;
      return net_failure;
    }
  }
  if(tcp_create_socket_base(socket) == net_failure) {
    (void) pthread_mutex_destroy(&socket->lock);
    return net_failure;
  }
  if(net_socket_base_options(socket->base.sfd) == net_failure) {
    tcp_socket_free(socket);
    return net_failure;
  }
  if(net_connect_socket(socket->base.sfd, &socket->base.addr) != 0 && errno != EINPROGRESS) {
    tcp_socket_free(socket);
    return net_failure;
  }
  if(net_epoll_add(socket->epoll, &socket->base) == net_failure) {
    tcp_socket_free(socket);
    return net_failure;
  }
  return net_success;
}

/* tcp_send() returns the amount of data sent. It might set errno to an error code.

If errno is EPIPE, no data was sent and no data may be sent in the future.
That may be because either we closed the channel, or the connection is closed.

If the function returns less bytes than requested, it might be for 2 reasons: either
the socket is closed and errno is set to EPIPE, or we are out of memory. If the
latter, the application shall retry the call with adjusted data pointer and size,
to reflect the data that wasn't sent or buffered.
*/

int tcp_send(struct tcp_socket* const socket, const void* data, int size) {
  if(tcp_socket_get_state(socket) == tcp_closed) {
    errno = ENOTCONN;
    return 0;
  } else if(tcp_socket_test_flag(socket, tcp_shutdown_wr)) {
    errno = EPIPE;
    return 0;
  }
  
  const unsigned cork = tcp_socket_test_flag(socket, tcp_cork) ? MSG_MORE : 0;
  int all = size;
  if(tcp_socket_test_flag(socket, tcp_can_send)) {
    /* We are allowed to send data. Do it instead of waiting for epoll to preserve memory. */
    while(1) {
      const ssize_t bytes = send(socket->base.sfd, data, size, MSG_NOSIGNAL | cork);
      if(bytes == -1) {
        switch(errno) {
          case EINTR: continue;
          case EAGAIN: {
            tcp_socket_clear_flag(socket, tcp_can_send);
            goto out;
          }
          case EPIPE: { /* We cannot send any more data */
            tcp_socket_set_flag(socket, tcp_shutdown_wr);
            tcp_socket_clear_flag(socket, tcp_can_send);
            goto ret;
          }
          default: {
            /* Wait until the next epoll iteration. If the error was fatal, the connection
            will be closed with an EPOLLHUP. Otherwise we can probably ignore it. */
            goto out;
          }
        }
      }
      size -= bytes;
      if(size == 0) {
        /* Wrote everything, best case scenario */
        errno = 0;
        return all;
      }
      data = (char*) data + bytes;
    }
  } else {
    errno = 0;
  }
  out:;
  unsigned buffered = 0;
  if(socket->settings->disable_send_buffer == 0) {
    /* If not everything was processed, store it for later */
    (void) pthread_mutex_lock(&socket->lock);
    if(socket->send_used + size > socket->send_size) {
      char* const ptr = realloc(socket->send_buffer, socket->send_used + size);
      if(ptr == NULL) {
        /* If resize failed, but there is some free space (buffered != 0), use it */
        buffered = socket->send_size - socket->send_used;
      } else {
        buffered = size;
      }
      socket->send_buffer = ptr;
      socket->send_size += buffered;
    } else {
      buffered = size;
    }
    /* Append the data */
    (void) memcpy(socket->send_buffer + socket->send_used, data, buffered);
    socket->send_used += buffered;
    (void) pthread_mutex_unlock(&socket->lock);
  }
  all -= buffered;
  ret:
  return all;
}

/* tcp_read() returns the amount of data read. It might set errno to an error code.

If errno is EPIPE, there is no more data to be read (but data might have been read).

The onreadclose() callback DOES NOT mean there is no more data to be read. It
only means that the peer won't send more data - we can close our channel to
gracefully close the TCP connection. Data might still be pending in the kernel
buffer, waiting to be collected by the application with tcp_read().
*/

int tcp_read(struct tcp_socket* const socket, void* data, int size) {
  /* We MUST NOT check if the socket is closed or closing for reading, because
  there still might be data pending in the kernel buffer that we might want.
  Doing this check just to not call recv() for nothing. Yes, there is a race
  condition, but this still gives *something*. Better than nothing. */
  if(tcp_socket_test_flag(socket, tcp_data_ended)) {
    errno = EPIPE;
    return 0;
  }
  const int all = size;
  while(1) {
    const ssize_t bytes = recv(socket->base.sfd, data, size, 0);
    if(bytes == -1) {
      switch(errno) {
        case EINTR: continue;
        case EAGAIN: {
          errno = 0;
          goto out;
        }
        default: {
          /* Wait until the next epoll iteration. If the error was fatal, the connection
          will be closed with an EPOLLHUP. Otherwise we can probably ignore it.
          Yes, the application could be notified about the error, but it doesn't need
          to check the errno whatsoever if the socket will be closed by epoll anyway.
          The application can just call tcp_send() and tcp_read() without worrying
          about the underlying socket's state. */
          goto out;
        }
      }
    } else if(bytes == 0) {
      /* End of data, let epoll deal with the socket */
      tcp_socket_set_flag(socket, tcp_data_ended);
      errno = EPIPE;
      break;
    }
    size -= bytes;
    if(size == 0) {
      /* Read everything, best case scenario */
      errno = 0;
      break;
    }
    /* If we read less than requested, we either have no more data in the
    kernel buffer, or a FIN has arrived. We MUST NOT assume that the next
    iteration there will still be no more data, because it might arrive in
    the meantime, so we MUST adjust the size and buffer pointer to not overflow.
    If there really is no more data to be read, the system call will fail in
    the next iteration. */
    data = (char*) data + bytes;
  }
  out:
  return all - size;
}

#define socket ((struct tcp_socket*) base)

/* A socket may only be in 1 epoll instance at a time. If the application wants
otherwise, it must supply a mutex and make a new onevent function which wraps
tcp_socket_onevent() in the mutex, so that it never is executed at the same time. */

static void tcp_socket_onevent(int events, struct net_socket_base* base) {
  if(events & EPOLLERR) {
    /* Check any errors we could have got from connect() or such */
    int code;
    if(getsockopt(socket->base.sfd, tcp_protocol, SO_ERROR, &code, &(socklen_t){sizeof(int)}) == 0) {
      errno = code;
    }
    tcp_socket_set_state_closed(socket);
    socket->callbacks->onclose(socket);
    return;
  }
  if(events & EPOLLHUP) {
    /* The connection is closed */
    tcp_socket_set_state_closed(socket);
    errno = 0;
    socket->callbacks->onclose(socket);
    return;
  }
  if((events & EPOLLOUT) || tcp_socket_test_flag(socket, tcp_can_send)) {
    if(events & EPOLLOUT) {
      tcp_socket_set_flag(socket, tcp_can_send);
    }
    if(!tcp_socket_test_flag(socket, tcp_opened)) {
      /* connect() succeeded */
      tcp_socket_set_state_connected(socket);
      tcp_socket_set_flag(socket, tcp_opened);
      if(socket->callbacks->onopen != NULL) {
        socket->callbacks->onopen(socket);
      }
    } /* There is no "else" statement, because the application could have
    scheduled some data to be sent in the onopen() function */
    (void) pthread_mutex_lock(&socket->lock);
    if(socket->settings->disable_send_buffer == 0 && socket->send_used > 0) {
      /* Simply send contents of our send_buffer in FIFO manner. We can't use
      tcp_send(), because upon failure it would buffer the already-buffered data
      we are trying to send. */
      int addon = 0;
      const unsigned cork = tcp_socket_test_flag(socket, tcp_cork) ? MSG_MORE : 0;
      while(1) {
        const ssize_t bytes = send(socket->base.sfd, socket->send_buffer + addon, socket->send_used, MSG_NOSIGNAL | cork);
        if(bytes == -1) {
          if(errno == EINTR) {
            continue;
          } else if(errno == EAGAIN) {
            /* If kernel doesn't accept more data, don't send more in the future */
            tcp_socket_clear_flag(socket, tcp_can_send);
          }
          /* Wait until the next epoll iteration. If the error was fatal, the connection
          will be closed with an EPOLLHUP. Otherwise we can probably ignore it. */
          break;
        }
        socket->send_used -= bytes;
        if(socket->send_used == 0) {
          break;
        }
        addon += bytes;
      }
      /* Shift the data we didn't send, maybe resize the buffer */
      if(socket->send_used != 0) {
        (void) memmove(socket->send_buffer, socket->send_buffer + addon, socket->send_used);
        const unsigned times = (socket->send_size - socket->send_used) / socket->settings->send_buffer_cleanup_threshold;
        if(times != 0) {
          char* const ptr = realloc(socket->send_buffer, socket->send_size - socket->settings->send_buffer_cleanup_threshold * times);
          /* DO NOT take any action if realloc failed. A realloc may fail shrinking
          a block of memory with a certain type of an allocator, which has buckets
          for different sizes of blocks, and if a bucket of the smaller size is full,
          even if there is a lot of free space in other buckets, it will fail.
          We leave this case up to the application. Either it will call tcp_send()
          and the function will return less bytes than requested, meaning an ENOMEM,
          or it will eventually free the socket after it is closed and the memory
          will vanish too. Either way, to not create more problems, we just keep
          the old big size of the buffer. */
          if(ptr != NULL) {
            socket->send_buffer = ptr;
            socket->send_size -= socket->settings->send_buffer_cleanup_threshold * times;
          }
        }
      } else if(socket->settings->send_buffer_allow_freeing == 1) {
        free(socket->send_buffer);
        socket->send_buffer = NULL;
        socket->send_size = 0;
      }
      (void) pthread_mutex_unlock(&socket->lock);
    } else {
      (void) pthread_mutex_unlock(&socket->lock);
    }
    if(socket->callbacks->onsend != NULL) {
      socket->callbacks->onsend(socket);
    }
  }
  /* For reading, we don't check for tcp_socket_test_flag(socket, tcp_can_read)
  just as we did above for sending, because it would behave like a malfunctioning
  level-triggered event. Instead, we make a simple rule: if the application's
  onmessage() callback is executed, the application can either read all of it
  (until EAGAIN or EPIPE) and don't worry about it further, or it can read it
  partially or not read it at all, but it's up to the application to read it
  later. This way, the application has multiple choices, but it also has greater
  responsibility if it wants to read later or not read at all.
  We also get rid of the tcp_can_read flag. */
  if(events & EPOLLIN) {
    if(socket->callbacks->onmessage != NULL) {
      socket->callbacks->onmessage(socket);
    }
  }
  if(events & EPOLLRDHUP) {
    /* We need to send back a FIN, but let the application choose the right moment */
    if(socket->settings->onreadclose_auto_res == 1) {
      tcp_socket_close(socket);
    }
    if(socket->callbacks->onreadclose != NULL) {
      socket->callbacks->onreadclose(socket);
    }
  }
}

#undef socket



void tcp_server_free(struct tcp_server* const server) {
  (void) pthread_rwlock_destroy(&server->lock);
  free(server->sockets);
  free(server->freeidx);
  (void) close(server->base.sfd);
  server->base.sfd = -1;
  const unsigned offset = offsetof(struct tcp_server, lock);
  (void) memset((char*) server + offset, 0, sizeof(struct tcp_server) - offset);
}

#define server ((struct tcp_server*) base)

static void tcp_server_shutdown_internal(struct net_socket_base* base) {
  if(tcp_server_get_conn_amount(server) > 0) {
    (void) pthread_rwlock_wrlock(&server->lock);
    for(unsigned i = 0; i < server->settings->max_conn; ++i) {
      if(server->sockets[i].base.sfd > 0) {
        (void) setsockopt(server->sockets[i].base.sfd, SOL_SOCKET, SO_LINGER, &((struct linger) { .l_onoff = 1, .l_linger = 0 }), sizeof(struct linger));
        (void) pthread_mutex_destroy(&server->sockets[i].lock);
        if(server->sockets[i].send_buffer != NULL) {
          free(server->sockets[i].send_buffer);
        }
        (void) close(server->sockets[i].base.sfd);
        if(atomic_fetch_sub(&server->connections, 1) == 1) {
          (void) pthread_rwlock_unlock(&server->lock);
          break;
        }
      }
    }
  }
  server->callbacks->onshutdown(server);
}

#undef server

int tcp_create_server_base(struct tcp_server* const server) {
  const int sfd = socket(net_get_family(&server->base.addr), stream_socktype, tcp_protocol);
  if(sfd == -1) {
    return net_failure;
  }
  server->base.sfd = sfd;
  server->base.which = net_server;
  server->base.events = EPOLLET | EPOLLIN;
  server->base.onclose = tcp_server_shutdown_internal;
  return net_success;
}

int tcp_create_server(struct tcp_server* const server) {
  if(server->sockets == NULL) {
    server->sockets = calloc(server->settings->max_conn, sizeof(struct tcp_socket));
    if(server->sockets == NULL) {
      return net_failure;
    }
  }
  if(server->freeidx == NULL) {
    server->freeidx = malloc(sizeof(unsigned) * server->settings->max_conn);
    if(server->freeidx == NULL) {
      free(server->sockets);
      server->sockets = NULL;
      return net_failure;
    }
  }
  const int ret = pthread_rwlock_init(&server->lock, NULL);
  if(ret != 0) {
    free(server->sockets);
    free(server->freeidx);
    server->sockets = NULL;
    server->freeidx = NULL;
    errno = ret;
    return net_failure;
  }
  if(tcp_create_server_base(server) == net_failure) {
    free(server->sockets);
    free(server->freeidx);
    server->sockets = NULL;
    server->freeidx = NULL;
    (void) pthread_rwlock_destroy(&server->lock);
    return net_failure;
  }
  if(net_socket_base_options(server->base.sfd) == net_failure) {
    tcp_server_free(server);
    return net_failure;
  }
  if(net_bind_socket(server->base.sfd, &server->base.addr) != 0 || listen(server->base.sfd, server->settings->backlog) != 0) {
    tcp_server_free(server);
    return net_failure;
  }
  if(net_epoll_add(server->epoll, &server->base) == net_failure) {
    tcp_server_free(server);
    return net_failure;
  }
  return net_success;
}

#define _server ((struct tcp_server*) base)

/* A server can be in multiple epoll instances. Since the epoll is edge-triggered,
one can specify a number higher than 1 to net_epoll_start() and bind threads it spawns
to various CPU cores to increase computing power if needed. Throughput can be increased
by creating multiple servers listening on the same port to let the kernel load-balance. */

void tcp_server_onevent(int events, struct net_socket_base* base) {
  /* If we got an event, it will for sure be a new connection. We employ a while loop
  to accept until we hit EAGAIN. That way, we won't cause accidental starvation. */
  tryagain:
  while(1) {
    /* Instead of getting an index for the socket, first make sure the connection isn't aborted */
    struct sockaddr_in6 addr;
    socklen_t addrlen = sizeof(struct sockaddr_in6);
    int sfd = accept(_server->base.sfd, &addr, &addrlen);
    if(sfd == -1) {
      if(errno == EAGAIN) {
        /* No more connections to accept */
        return;
      } else if(errno == ENOMEM && _server->callbacks->onnomem(_server) == net_success) {
        /* Case where the application has a mechanism for decreasing memory usage */
        continue;
      }
      /* An error we can't fix (either connection error or limit on something) */
      goto tryagain;
    }
    (void) pthread_rwlock_wrlock(&_server->lock);
    /* First check if we hit the connection limit for the server, or if we don't
    accept new connections. If yes, just close the socket. This way, it is removed
    from the queue of pending connections and it won't get timed out, wasting system resources. */
    if(_server->settings->max_conn == tcp_server_get_conn_amount(_server) || aflag32_test(&_server->flags, tcp_disallow_connections)) {
      (void) pthread_rwlock_unlock(&_server->lock);
      (void) setsockopt(sfd, SOL_SOCKET, SO_LINGER, &((struct linger) { .l_onoff = 1, .l_linger = 0 }), sizeof(struct linger));
      (void) close(sfd);
      goto tryagain;
    }
    /* Otherwise, get memory for the socket */
    struct tcp_socket* socket;
    /* First check if there are any free indexes. If not, we will just go with
    sockets_used. Note that we don't modify anything here. That is to not have
    to return the socket index if anything fails later on. We have the rwlock
    for ourselves for the whole duration of the socket's creation, so might as
    well use it. Otherwise, we would need to waste time returning the index. */
    if(_server->freeidx_used == 0) {
      socket = _server->sockets + _server->sockets_used;
    } else {
      socket = _server->sockets + _server->freeidx[_server->freeidx_used - 1];
    }
    /* Now we proceed to initialise required members of the socket. We don't
    have to zero it - it should already be zeroed. */
    socket->base.sfd = sfd;
    socket->base.which = net_socket;
    socket->base.events = EPOLLET | EPOLLRDHUP | EPOLLIN | EPOLLOUT;
    socket->server = _server;
    socket->epoll = _server->epoll;
    net_sockbase_set_whole_addr(&socket->base, &addr);
    if(_server->callbacks->onconnection(_server, socket) == tcp_fatal) {
      (void) memset(socket, 0, sizeof(struct tcp_socket));
      (void) pthread_rwlock_unlock(&_server->lock);
      (void) setsockopt(sfd, SOL_SOCKET, SO_LINGER, &((struct linger) { .l_onoff = 1, .l_linger = 0 }), sizeof(struct linger));
      (void) close(sfd);
      goto tryagain;
    }
    while(1) {
      if(socket->settings->disable_send_buffer == 0) {
        const int err = pthread_mutex_init(&socket->lock, NULL);
        if(err != 0) {
          errno = err;
          if(errno == ENOMEM && _server->callbacks->onnomem(_server) == net_success) {
            continue;
          } else {
            if(_server->callbacks->ontermination != NULL) {
              _server->callbacks->ontermination(_server, socket);
            }
            (void) memset(socket, 0, sizeof(struct tcp_socket));
            (void) pthread_rwlock_unlock(&_server->lock);
            (void) close(sfd);
            goto tryagain;
          }
        }
      }
      break;
    }
    if(net_socket_base_options(sfd) == net_failure) {
      if(_server->callbacks->ontermination != NULL) {
        _server->callbacks->ontermination(_server, socket);
      }
      (void) pthread_mutex_destroy(&socket->lock);
      (void) memset(socket, 0, sizeof(struct tcp_socket));
      (void) pthread_rwlock_unlock(&_server->lock);
      (void) close(sfd);
      goto tryagain;
    }
    while(1) {
      if(net_epoll_add(socket->epoll, &socket->base) == net_failure) {
        if(errno == ENOMEM && _server->callbacks->onnomem(_server) == net_success) {
          continue;
        } else {
          if(_server->callbacks->ontermination != NULL) {
            _server->callbacks->ontermination(_server, socket);
          }
          (void) pthread_mutex_destroy(&socket->lock);
          (void) memset(socket, 0, sizeof(struct tcp_socket));
          (void) pthread_rwlock_unlock(&_server->lock);
          (void) close(sfd);
          goto tryagain;
        }
      }
      break;
    }
    /* At this point, the socket is fully initialised. We unlock the rwlock only
    now and not after done getting memory for the socket so that if the connection
    gets closed here, we don't trigger undefined behavior. */
    if(_server->freeidx_used == 0) {
      ++_server->sockets_used;
    } else {
      --_server->freeidx_used;
    }
    (void) atomic_fetch_add(&_server->connections, 1);
    (void) pthread_rwlock_unlock(&_server->lock);
  }
}

#undef _server

/* We allow the application to access the connections, but due to rwlock and contmem,
we will do it by ourselves so that the application doesn't mess up somewhere. Note
that order in which the sockets are accessed is not linear and the application should
never remember anything index-related, because item order changes as sockets are
destroyed.
If the application wishes to do something send_buffer-related, it MUST first acquire
the socket's mutex by doing pthread_mutex_lock(&socket->lock), otherwise the behavior
is undefined. It must only do so if socket->settings->disable_send_buffer == 0, or
else the mutex won't be initialised and the application won't be able to lock it.
The behavior is undefined if the callback function will attempt to free the server. */

void tcp_server_foreach_conn(struct tcp_server* const server, void (*callback)(struct tcp_socket*, void*), void* data, const int write) {
  if(write) {
    (void) pthread_rwlock_wrlock(&server->lock);
  } else {
    (void) pthread_rwlock_rdlock(&server->lock);
  }
  for(unsigned i = 0; i < server->settings->max_conn; ++i) {
    if(server->sockets[i].base.sfd != 0) {
      callback(server->sockets + i, data);
    }
  }
  (void) pthread_rwlock_unlock(&server->lock);
}

void tcp_server_dont_accept_conn(struct tcp_server* const server) {
  aflag32_add(&server->flags, tcp_disallow_connections);
}

void tcp_server_accept_conn(struct tcp_server* const server) {
  aflag32_del(&server->flags, tcp_disallow_connections);
}

/* Asynchronously shutdown the server - remove it from it's epoll, close all of
it's sockets. The sockets MUST be freed for this to succeed and call the
onshutdown() callback, so they MUST call tcp_socket_free() from within onclose(). */

int tcp_server_shutdown(struct tcp_server* const server) {
  (void) pthread_rwlock_wrlock(&server->lock);
  aflag32_add(&server->flags, tcp_disallow_connections | tcp_server_closing);
  (void) pthread_rwlock_unlock(&server->lock);
  return net_epoll_safe_remove(server->epoll, &server->base);
}

unsigned tcp_server_get_conn_amount(const struct tcp_server* const server) {
  return atomic_load(&server->connections);
}

static void tcp_onevent(struct net_epoll* epoll, int events, struct net_socket_base* base) {
  if(base->which == net_socket) {
    tcp_socket_onevent(events, base);
  } else {
    tcp_server_onevent(events, base);
  }
}

int tcp_epoll(struct net_epoll* const epoll) {
  return net_epoll(epoll, tcp_onevent, net_epoll_wakeup_method);
}