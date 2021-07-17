#include "tcp.h"
#include "debug.h"
#include "aflags.h"

#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <netinet/tcp.h>

static struct tcp_socket_settings tcp_socket_settings = { 0, 1, 1, 1, 0, 0 };

static struct tcp_socket_settings tcp_serversock_settings = { 0, 1, 0, 1, 0, 0 };

static struct tcp_server_settings tcp_server_settings = { 100, 64 };

static inline void tcp_socket_set_flag(struct tcp_socket* const socket, const uint32_t flag) {
  aflag32_add(&socket->flags, flag);
}

static inline uint32_t tcp_socket_test_flag(const struct tcp_socket* const socket, const uint32_t flag) {
  return aflag32_test(&socket->flags, flag);
}

static inline void tcp_socket_clear_flag(struct tcp_socket* const socket, const uint32_t flag) {
  aflag32_del(&socket->flags, flag);
}



void tcp_socket_cork_on(struct tcp_socket* const socket) {
  tcp_socket_set_flag(socket, tcp_cork);
}

void tcp_socket_cork_off(struct tcp_socket* const socket) {
  tcp_socket_clear_flag(socket, tcp_cork);
}

static inline int tcp_socket_keepalive_internal(const int sfd, const int retries, const int idle_time, const int reprobe_time) {
  if(net_socket_setopt_true(sfd, SOL_SOCKET, SO_KEEPALIVE) != 0) {
    return -1;
  }
  if(setsockopt(sfd, IPPROTO_TCP, TCP_KEEPCNT, &retries, sizeof(int)) != 0) {
    return -1;
  }
  if(setsockopt(sfd, IPPROTO_TCP, TCP_KEEPIDLE, &idle_time, sizeof(int)) != 0) {
    return -1;
  }
  if(setsockopt(sfd, IPPROTO_TCP, TCP_KEEPINTVL, &reprobe_time, sizeof(int)) != 0) {
    return -1;
  }
  return 0;
}

int tcp_socket_keepalive(const struct tcp_socket* const socket) {
  return tcp_socket_keepalive_internal(socket->base.sfd, 10, 1, 1);
}

int tcp_socket_keepalive_explicit(const struct tcp_socket* const socket, const int retries, const int idle_time, const int reprobe_time) {
  return tcp_socket_keepalive_internal(socket->base.sfd, retries, idle_time, reprobe_time);
}

static inline void tcp_socket_no_linger(const int sfd) {
  (void) setsockopt(sfd, SOL_SOCKET, SO_LINGER, &((struct linger) { .l_onoff = 1, .l_linger = 0 }), sizeof(struct linger));
}

void tcp_socket_stop_receiving_data(struct tcp_socket* const socket) {
  tcp_socket_set_flag(socket, tcp_data_ended);
  (void) shutdown(socket->base.sfd, SHUT_RD);
}



/* This function MUST only be called from within the onclose() callback or after
it occurs, NOT BEFORE */

void tcp_socket_free(struct tcp_socket* const socket) {
  if(socket->server != NULL) {
    /* By design, the server is either tcp_server_closing, or it is not. It can't
    become closing in this function. That means we don't have to lock it's rwlock.
    Additionally, since the order update in net_epoll, socket deletions will always
    be handled before server shutdowns, so we are actually GUARANTEED here that
    the server isn't closing. That means we can simply ignore the check and don't
    need to check if there are 0 connections to call the onshutdown callback. */
    struct tcp_server* const server = socket->server;
    (void) pthread_rwlock_wrlock(&server->lock);
    if(socket->callbacks->onfree != NULL) {
      socket->callbacks->onfree(socket);
    }
    (void) pthread_mutex_destroy(&socket->lock);
    if(socket->send_buffer != NULL) {
      free(socket->send_buffer);
    }
    if(socket->base.sfd != -1) {
      (void) close(socket->base.sfd);
    }
    /* Return the socket's index for reuse */
    const uint32_t index = ((uintptr_t) socket - server->settings->offset - (uintptr_t) server->sockets) / server->socket_size;
    server->freeidx[server->freeidx_used++] = index;
    (void) memset(socket, 0, server->socket_size);
    (void) pthread_rwlock_unlock(&server->lock);
  } else {
    if(socket->callbacks->onfree != NULL) {
      /* In case the socket will try to destroy epoll we are in right now */
      (void) pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);
      socket->callbacks->onfree(socket);
    }
    (void) pthread_mutex_destroy(&socket->lock);
    if(socket->send_buffer != NULL) {
      free(socket->send_buffer);
    }
    if(socket->base.sfd != -1) {
      (void) close(socket->base.sfd);
      socket->base.sfd = -1;
    }
    socket->base.which = net_unspecified;
    socket->base.events = 0;
    if(socket->settings->free_on_free) {
      free((char*) socket - socket->settings->free_offset);
    } else {
      const int offset = offsetof(struct tcp_socket, lock);
      (void) memset((char*) socket + offset, 0, sizeof(struct tcp_socket) - offset);
    }
  }
}

/* Attempt to gracefully close the connection */

void tcp_socket_close(struct tcp_socket* const socket) {
  tcp_socket_set_flag(socket, tcp_shutdown_wr);
  tcp_socket_clear_flag(socket, tcp_can_send);
  (void) shutdown(socket->base.sfd, SHUT_WR);
}

/* Not gracefully, but quicker */

void tcp_socket_force_close(struct tcp_socket* const socket) {
  tcp_socket_set_flag(socket, tcp_shutdown_wr);
  tcp_socket_clear_flag(socket, tcp_can_send);
  tcp_socket_no_linger(socket->base.sfd);
  (void) shutdown(socket->base.sfd, SHUT_RDWR);
}



int tcp_create_socket(struct tcp_socket* const sock) {
  if(sock->settings == NULL) {
    sock->settings = &tcp_socket_settings;
  }
  sock->cur_info = sock->info;
  while(1) {
    net_sockbase_set_whole_addr(&sock->base, net_addrinfo_get_whole_addr(sock->cur_info));
    sock->cur_info = sock->cur_info->ai_next;
    {
      const int sfd = socket(net_get_family(&sock->base.addr), stream_socktype, tcp_protocol);
      if(sfd == -1) {
        if(sock->cur_info != NULL) {
          continue;
        }
        return -1;
      }
      sock->base.sfd = sfd;
      sock->base.which = net_socket;
      sock->base.events = EPOLLET | EPOLLRDHUP | EPOLLIN | EPOLLOUT;
      sock->base.onclose = NULL; /* We don't close sockets using safe_remove */
    }
    if(net_socket_base_options(sock->base.sfd) != 0) {
      if(sock->cur_info != NULL) {
        (void) close(sock->base.sfd);
        continue;
      }
      goto err_sock;
    }
    if(net_connect_socket(sock->base.sfd, &sock->base.addr) != 0 && errno != EINPROGRESS) {
      if(sock->cur_info != NULL) {
        (void) close(sock->base.sfd);
        continue;
      }
      goto err_sock;
    }
    break;
  }
  {
    const int ret = pthread_mutex_init(&sock->lock, NULL);
    if(ret != 0) {
      errno = ret;
      goto err_sock;
    }
  }
  /* Pretty late so that we don't have to call onfree() numerous times above */
  if(sock->callbacks->oncreation != NULL && sock->callbacks->oncreation(sock) != 0) {
    goto err_mutex;
  }
  if(net_epoll_add(sock->epoll, &sock->base) != 0) {
    /* Makes sense to use onfree() only if oncreation() is set too */
    if(sock->callbacks->onfree != NULL) {
      sock->callbacks->onfree(sock);
    }
    goto err_mutex;
  }
  return 0;
  
  err_mutex:
  (void) pthread_mutex_destroy(&sock->lock);
  (void) memset(&sock->lock, 0, sizeof(pthread_mutex_t));
  err_sock:
  (void) close(sock->base.sfd);
  /* Don't zero the address */
  sock->base.sfd = -1;
  sock->base.which = net_unspecified;
  sock->base.events = 0;
  return -1;
}

/* tcp_send() returns the amount of data sent. It might set errno to an error code.

If errno is EPIPE, no data may be sent in the future and partial or no data was sent.
That may be because either we closed the channel, or the connection is closed.
*/

int tcp_send(struct tcp_socket* const socket, const void* data, int size) {
  if(tcp_socket_test_flag(socket, tcp_shutdown_wr)) {
    errno = EPIPE;
    return 0;
  }
  const uint32_t cork = tcp_socket_test_flag(socket, tcp_cork) ? MSG_MORE : 0;
  (void) pthread_mutex_lock(&socket->lock);
  if(socket->send_used > 0) {
    int addon = 0;
    while(1) {
      const size_t bytes = send(socket->base.sfd, socket->send_buffer + addon, socket->send_used, MSG_NOSIGNAL | cork);
      if(bytes == -1) {
        if(errno == EAGAIN) {
          tcp_socket_clear_flag(socket, tcp_can_send);
        } else if(errno == EINTR) {
          continue;
        } else if(errno == EPIPE) {
          tcp_socket_set_flag(socket, tcp_shutdown_wr);
          tcp_socket_clear_flag(socket, tcp_can_send);
          free(socket->send_buffer);
          socket->send_buffer = NULL;
          socket->send_used = 0;
          socket->send_size = 0;
          (void) pthread_mutex_unlock(&socket->lock);
          return 0;
        }
        break;
      }
      socket->send_used -= bytes;
      if(socket->send_used == 0) {
        break;
      }
      addon += bytes;
    }
    if(socket->send_used != 0) {
      (void) memmove(socket->send_buffer, socket->send_buffer + addon, socket->send_used);
      if(socket->send_size - socket->send_used >= socket->settings->send_buffer_cleanup_threshold) {
        char* const ptr = realloc(socket->send_buffer, socket->send_used);
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
  }
  (void) pthread_mutex_unlock(&socket->lock);
  const int all = size;
  if(tcp_socket_test_flag(socket, tcp_can_send)) {
    /* We are allowed to send data. Do it instead of waiting for epoll to preserve memory. */
    while(1) {
      const size_t bytes = send(socket->base.sfd, data, size, MSG_NOSIGNAL | cork);
      if(bytes == -1) {
        if(errno == EAGAIN) {
          tcp_socket_clear_flag(socket, tcp_can_send);
        } else if(errno == EINTR) {
          continue;
        } else if(errno == EPIPE) {
          tcp_socket_set_flag(socket, tcp_shutdown_wr);
          tcp_socket_clear_flag(socket, tcp_can_send);
          (void) pthread_mutex_lock(&socket->lock);
          if(socket->send_buffer != NULL) {
            free(socket->send_buffer);
            socket->send_buffer = NULL;
          }
          socket->send_used = 0;
          socket->send_size = 0;
          (void) pthread_mutex_unlock(&socket->lock);
          return all - size;
        }
        /* Wait until the next epoll iteration. If the error was fatal, the connection
        will be closed with an EPOLLHUP. Otherwise we can probably ignore it. */
        break;
      }
      size -= bytes;
      if(size == 0) {
        /* Wrote everything, best case scenario */
        errno = 0;
        return all;
      }
      data = (char*) data + bytes;
    }
  }
  /* If not everything was processed, store it for later */
  (void) pthread_mutex_lock(&socket->lock);
  if(socket->send_used + size > socket->send_size) {
    while(1) {
      char* const ptr = realloc(socket->send_buffer, socket->send_used + size);
      if(ptr != NULL) {
        socket->send_buffer = ptr;
        socket->send_size = socket->send_used + size;
      } else if(socket->callbacks->onnomem(socket) == 0) {
        continue;
      } else {
        (void) pthread_mutex_unlock(&socket->lock);
        errno = ENOMEM;
        return 0;
      }
    }
  }
  /* Append the data */
  (void) memcpy(socket->send_buffer + socket->send_used, data, size);
  socket->send_used += size;
  (void) pthread_mutex_unlock(&socket->lock);
  return all;
}

/* tcp_read() returns the amount of data read. It might set errno to an error code.

If errno is EPIPE, there is no more data to be read (but data might have been read).
The same goes for EAGAIN, but EAGAIN is retryable.

The onreadclose() callback DOES NOT mean there is no more data to be read. It
only means that the peer won't send more data - we can close our channel to
gracefully close the TCP connection. Data might still be pending in the kernel
buffer, waiting to be collected by the application with tcp_read(). */

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
    const size_t bytes = recv(socket->base.sfd, data, size, 0);
    if(bytes == -1) {
      if(errno == EINTR) {
        continue;
      }
      /* Wait until the next epoll iteration. If the error was fatal, the connection
      will be closed with an EPOLLHUP. Otherwise we can probably ignore it.
      Yes, the application could be notified about the error, but it doesn't need
      to check the errno whatsoever if the socket will be closed by epoll anyway.
      The application can just call tcp_send() and tcp_read() without worrying
      about the underlying socket's state. */
      break;
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
  return all - size;
}

#define socket ((struct tcp_socket*) base)

/* A socket may only be in 1 epoll instance at a time */

static void tcp_socket_onevent(int events, struct net_socket_base* base) {
  if(events & EPOLLERR) {
    if(tcp_socket_test_flag(socket, tcp_opened)) {
      /* Weird, but maybe can happen, who knows */
      goto epollhup;
    }
    if(socket->cur_info == NULL) {
      int code;
      if(getsockopt(socket->base.sfd, SOL_SOCKET, SO_ERROR, &code, &(socklen_t){sizeof(int)}) == 0) {
        errno = code;
      } else {
        errno = -1;
      }
      (void) close(socket->base.sfd);
      socket->base.sfd = -1;
    } else {
      beginning:
      (void) close(socket->base.sfd);
      socket->base.sfd = -1;
      /* Maybe other addresses will work, let's try to reroute without
      reinitializing the whole socket from scratch */
      int reason;
      net_sockbase_set_whole_addr(&socket->base, net_addrinfo_get_whole_addr(socket->cur_info));
      struct addrinfo* save = socket->cur_info;
      socket->cur_info = socket->cur_info->ai_next;
      while(1) {
#undef socket
        const int sfd = socket(net_get_family(&base->addr), stream_socktype, tcp_protocol);
#define socket ((struct tcp_socket*) base)
        if(sfd == -1) {
          if(errno == ENOMEM && socket->callbacks->onnomem(socket) == 0) {
            continue;
          }
          reason = errno;
          goto err;
        }
        socket->base.sfd = sfd;
        break;
      }
      while(net_socket_base_options(socket->base.sfd) != 0) {
        if(errno != ENOMEM || socket->callbacks->onnomem(socket) != 0) {
          reason = errno;
          goto err;
        }
      }
      while(net_connect_socket(socket->base.sfd, &socket->base.addr) != 0 && errno != EINPROGRESS) {
        if(errno != ENOMEM || socket->callbacks->onnomem(socket) != 0) {
          reason = errno;
          goto err;
        }
      }
      while(net_epoll_add(socket->epoll, &socket->base) != 0) {
        if(errno != ENOMEM || socket->callbacks->onnomem(socket) != 0) {
          reason = errno;
          goto err;
        }
      }
      /* Success rerouting the socket */
      return;
      
      err:
      if(reason == ENOMEM) {
        /* Little chances that we will succeed next time even if we still
        have addresses to connect to */
        if(socket->base.sfd != -1) {
          (void) close(socket->base.sfd);
        }
        errno = reason;
      } else {
        /* Maybe there is hope */
        socket->cur_info = save;
        goto beginning;
      }
    }
    if(!socket->settings->dont_free_addrinfo) {
      net_get_address_free(socket->info);
    }
    socket->callbacks->onclose(socket);
    return;
  }
  if(events & EPOLLHUP) {
    epollhup:
    /* The connection is closed. Epoll sometimes likes to report EPOLLHUP twice,
    which is obviously a terrible idea if the application doesn't remove the socket
    from the epoll. That's why we will do it ourselves now if required. */
    if(socket->settings->remove_from_epoll_onclose) {
      (void) net_epoll_remove(socket->epoll, &socket->base);
    }
    if(!socket->settings->dont_free_addrinfo) {
      net_get_address_free(socket->info);
    }
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
      tcp_socket_set_flag(socket, tcp_opened);
      if(socket->callbacks->onopen != NULL) {
        socket->callbacks->onopen(socket);
      }
    } /* There is no "else" statement, because the application could have
    scheduled some data to be sent in the onopen() function */
    (void) pthread_mutex_lock(&socket->lock);
    if(socket->send_used > 0) {
      /* Simply send contents of our send_buffer in FIFO manner. We can't use
      tcp_send(), because upon failure it would buffer the already-buffered data
      we are trying to send. */
      int addon = 0;
      const uint32_t cork = tcp_socket_test_flag(socket, tcp_cork) ? MSG_MORE : 0;
      while(1) {
        const ssize_t bytes = send(socket->base.sfd, socket->send_buffer + addon, socket->send_used, MSG_NOSIGNAL | cork);
        if(bytes == -1) {
          if(errno == EAGAIN) {
            /* If the kernel doesn't accept more data, don't send more in the future */
            tcp_socket_clear_flag(socket, tcp_can_send);
          } else if(errno == EINTR) {
            continue;
          } else if(errno == EPIPE) {
            tcp_socket_set_flag(socket, tcp_shutdown_wr);
            tcp_socket_clear_flag(socket, tcp_can_send);
            socket->send_used = 0;
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
        if(socket->send_size - socket->send_used >= socket->settings->send_buffer_cleanup_threshold) {
          char* const ptr = realloc(socket->send_buffer, socket->send_used);
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
    }
    (void) pthread_mutex_unlock(&socket->lock);
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
    if(socket->callbacks->onreadclose != NULL) {
      socket->callbacks->onreadclose(socket);
    }
    if(socket->settings->onreadclose_auto_res == 1) {
      tcp_socket_close(socket);
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
  server->base.which = net_unspecified;
  server->base.events = 0;
  server->base.onclose = NULL;
  const uint32_t offset = offsetof(struct tcp_server, lock);
  (void) memset((char*) server + offset, 0, sizeof(struct tcp_server) - offset);
}

static void tcp_server_shutdown_socket_close(struct tcp_socket* socket, void* data) {
  (void) data;
  if(socket->callbacks->onfree != NULL) {
    socket->callbacks->onfree(socket);
  }
  tcp_socket_no_linger(socket->base.sfd);
  (void) pthread_mutex_destroy(&socket->lock);
  if(socket->send_buffer != NULL) {
    free(socket->send_buffer);
  }
  (void) close(socket->base.sfd);
}

#define server ((struct tcp_server*) base)

static void tcp_server_shutdown_internal(struct net_socket_base* base) {
  tcp_server_foreach_conn(server, tcp_server_shutdown_socket_close, NULL, 1);
  /* In case the server will try to destroy epoll we are in right now */
  (void) pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);
  server->callbacks->onshutdown(server);
}

#undef server

int tcp_create_server(struct tcp_server* const server, struct addrinfo* const info) {
  if(server->settings == NULL) {
    server->settings = &tcp_server_settings;
  }
  if(server->socket_size == 0) {
    server->socket_size = sizeof(struct tcp_socket);
  }
  if(server->sockets == NULL) {
    server->sockets = calloc(server->settings->max_conn, server->socket_size);
    if(server->sockets == NULL) {
      return -1;
    }
  }
  if(server->freeidx == NULL) {
    server->freeidx = malloc(sizeof(uint32_t) * server->settings->max_conn);
    if(server->freeidx == NULL) {
      goto sockets;
    }
  }
  {
    const int ret = pthread_rwlock_init(&server->lock, NULL);
    if(ret != 0) {
      errno = ret;
      goto freeidx;
    }
  }
  struct addrinfo* cur_info = info;
  while(1) {
    net_sockbase_set_whole_addr(&server->base, net_addrinfo_get_whole_addr(cur_info));
    cur_info = cur_info->ai_next;
    {
      const int sfd = socket(net_get_family(&server->base.addr), stream_socktype, tcp_protocol);
      if(sfd == -1) {
        if(cur_info == NULL) {
          goto rwlock;
        }
        continue;
      }
      server->base.sfd = sfd;
      server->base.which = net_server;
      server->base.events = EPOLLET | EPOLLIN;
      server->base.onclose = tcp_server_shutdown_internal;
    }
    if(net_socket_base_options(server->base.sfd) != 0) {
      goto maybe_retry;
    }
    if(net_bind_socket(server->base.sfd, &server->base.addr) != 0 || listen(server->base.sfd, server->settings->backlog) != 0) {
      goto maybe_retry;
    }
    break;
    
    maybe_retry:
    if(cur_info == NULL) {
      tcp_server_free(server);
      return -1;
    }
  }
  if(net_epoll_add(server->epoll, &server->base) != 0) {
    tcp_server_free(server);
    return -1;
  }
  return 0;
  
  rwlock:
  (void) pthread_rwlock_destroy(&server->lock);
  (void) memset(&server->lock, 0, sizeof(pthread_rwlock_t));
  freeidx:
  free(server->freeidx);
  server->freeidx = NULL;
  sockets:
  free(server->sockets);
  server->sockets = NULL;
  return -1;
}

#define _server ((struct tcp_server*) base)

/* Throughput can be increased by creating multiple servers listening on the same
port to let the kernel load-balance. For larger amount of servers, own load
balancer might be needed. */

void tcp_server_onevent(int events, struct net_socket_base* base) {
  /* If we got an event, it will for sure be a new connection. We employ a while loop
  to accept until we hit EAGAIN. That way, we won't cause accidental starvation. */
  while(1) {
    /* Instead of getting an index for the socket, first make sure the connection
    isn't aborted */
    struct sockaddr_in6 addr;
    socklen_t addrlen = sizeof(struct sockaddr_in6);
    int sfd = accept(_server->base.sfd, (struct sockaddr*)&addr, &addrlen);
    if(sfd == -1) {
      if(errno == EAGAIN) {
        /* No more connections to accept */
        return;
      } else if(errno == ENOMEM) {
        /* Case where the application has a mechanism for decreasing memory usage.
        Regardless of if it succeeds, we need to go to the next iteration anyway. */
        (void) _server->callbacks->onnomem(_server);
      } else if(errno == EINVAL) {
        die("tcp server's file descriptor is corrupted or a socket was mistakenly given a server flag");
      }
      /* Else, an error we can't fix (either connection error or limit on something) */
      continue;
    }
    /* First check if we hit the connection limit for the server, or if we don't
    accept new connections. If yes, just close the socket. This way, it is removed
    from the queue of pending connections and it won't get timed out, wasting system
    resources. */
    (void) pthread_rwlock_wrlock(&_server->lock);
    if(_server->settings->max_conn == tcp_server_get_conn_amount_raw(_server) || _server->disallow_connections == 1) {
      goto err_sock;
    }
    /* Otherwise, get memory for the socket */
    struct tcp_socket* socket;
    /* First check if there are any free indexes. If not, we will just go with
    sockets_used. Note that we don't modify anything here. That is to not have
    to return the socket index if anything fails later on. We have the rwlock
    for ourselves for the whole duration of the socket's creation, so might as
    well use it. */
    if(_server->freeidx_used == 0) {
      socket = (struct tcp_socket*)(_server->sockets + _server->sockets_used * _server->socket_size + _server->settings->offset);
    } else {
      socket = (struct tcp_socket*)(_server->sockets + _server->freeidx[_server->freeidx_used - 1] * _server->socket_size + _server->settings->offset);
    }
    /* Now we proceed to initialise required members of the socket. We don't
    have to zero it - it should already be zeroed. */
    socket->base.sfd = sfd;
    socket->base.which = net_socket;
    socket->base.events = EPOLLET | EPOLLRDHUP | EPOLLIN | EPOLLOUT;
    socket->server = _server;
    socket->epoll = _server->epoll;
    net_sockbase_set_whole_addr(&socket->base, &addr);
    if(_server->callbacks->onconnection(socket) != 0) {
      goto err_mem;
    }
    if(socket->settings == NULL) {
      socket->settings = &tcp_serversock_settings;
    }
    while(1) {
      const int err = pthread_mutex_init(&socket->lock, NULL);
      if(err != 0) {
        if(err == ENOMEM && _server->callbacks->onnomem(_server) == 0) {
          continue;
        } else {
          if(socket->callbacks->onfree != NULL) {
            socket->callbacks->onfree(socket);
          }
          goto err_mem;
        }
      }
      break;
    }
    if(net_socket_base_options(sfd) != 0) {
      if(socket->callbacks->onfree != NULL) {
        socket->callbacks->onfree(socket);
      }
      (void) pthread_mutex_destroy(&socket->lock);
      goto err_mem;
    }
    while(1) {
      if(net_epoll_add(socket->epoll, &socket->base) != 0) {
        if(errno == ENOMEM && _server->callbacks->onnomem(_server) == 0) {
          continue;
        } else {
          if(socket->callbacks->onfree != NULL) {
            socket->callbacks->onfree(socket);
          }
          (void) pthread_mutex_destroy(&socket->lock);
          goto err_mem;
        }
      }
      break;
    }
    /* At this point, the socket is fully initialised */
    if(_server->freeidx_used == 0) {
      ++_server->sockets_used;
    } else {
      --_server->freeidx_used;
    }
    (void) pthread_rwlock_unlock(&_server->lock);
    continue;
    
    err_mem:
    (void) memset(socket, 0, sizeof(struct tcp_socket));
    err_sock:
    (void) pthread_rwlock_unlock(&_server->lock);
    tcp_socket_no_linger(sfd);
    (void) close(sfd);
  }
}

#undef _server

#define socket ((struct tcp_socket*)(server->sockets + i * server->socket_size))

/* We allow the application to access the connections, but we will do it by ourselves
so that the application doesn't mess up somewhere. Note that order in which the
sockets are accessed is not linear.
If the application wishes to do something send_buffer-related, it MUST first acquire
the socket's mutex by doing pthread_mutex_lock(&socket->lock), otherwise the behavior
is undefined.
The behavior is undefined if the callback function will attempt to shutdown or free
the server.
The program will deadlock if the callback function tries calling
tcp_server_get_conn_amount(). It should instead call tcp_server_get_conn_amount_raw(). */

void tcp_server_foreach_conn(struct tcp_server* const server, void (*callback)(struct tcp_socket*, void*), void* data, const int write) {
  if(write) {
    (void) pthread_rwlock_wrlock(&server->lock);
  } else {
    (void) pthread_rwlock_rdlock(&server->lock);
  }
  uint32_t amount = tcp_server_get_conn_amount_raw(server);
  if(amount != 0) {
    for(uint32_t i = 0; i < server->settings->max_conn; ++i) {
      if(socket->base.sfd != 0) {
        callback(socket, data);
        if(--amount == 0) {
          break;
        }
      }
    }
  }
  (void) pthread_rwlock_unlock(&server->lock);
}

#undef socket

void tcp_server_dont_accept_conn(struct tcp_server* const server) {
  (void) pthread_rwlock_wrlock(&server->lock);
  server->disallow_connections = 1;
  (void) pthread_rwlock_unlock(&server->lock);
}

void tcp_server_accept_conn(struct tcp_server* const server) {
  (void) pthread_rwlock_wrlock(&server->lock);
  server->disallow_connections = 0;
  (void) pthread_rwlock_unlock(&server->lock);
}

/* Asynchronously shutdown the server - remove it from it's epoll, close all of
it's sockets. If there are still any connected sockets bound to this server, they
will be quietly dropped (without onclose(), but with onfree()) and their resources
will be freed. Thus, the application MUST NOT do any kind of operations on the
sockets or the server in the meantime. */

int tcp_server_shutdown(struct tcp_server* const server) {
  (void) pthread_rwlock_wrlock(&server->lock);
  server->is_closing = 1;
  (void) pthread_rwlock_unlock(&server->lock);
  return net_epoll_safe_remove(server->epoll, &server->base);
}

uint32_t tcp_server_get_conn_amount_raw(const struct tcp_server* const server) {
  return server->sockets_used - server->freeidx_used;
}

uint32_t tcp_server_get_conn_amount(struct tcp_server* const server) {
  (void) pthread_rwlock_wrlock(&server->lock);
  const uint32_t connections = tcp_server_get_conn_amount_raw(server);
  (void) pthread_rwlock_unlock(&server->lock);
  return connections;
}

static void tcp_onevent(struct net_epoll* epoll, int events, struct net_socket_base* base) {
  if(base->which == net_socket) {
    tcp_socket_onevent(events, base);
  } else {
    tcp_server_onevent(events, base);
  }
}

int tcp_epoll(struct net_epoll* const epoll) {
  epoll->on_event = tcp_onevent;
  return net_epoll(epoll, net_epoll_wakeup_method);
}