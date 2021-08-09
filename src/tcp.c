#include "tcp.h"
#include "debug.h"
#include "aflags.h"

#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <netinet/tcp.h>

static struct tcp_socket_settings tcp_socket_settings = { 1, 0, 0, 0, 0, 0, 1, 0 };

static struct tcp_server_settings tcp_server_settings = { 100, 64 };

static inline void tcp_socket_set_flag(struct tcp_socket* const socket, const uint32_t flag) {
  aflag_add2(&socket->flags, flag);
}

static inline uint32_t tcp_socket_test_flag(const struct tcp_socket* const socket, const uint32_t flag) {
  return aflag_test2(&socket->flags, flag);
}

static inline void tcp_socket_clear_flag(struct tcp_socket* const socket, const uint32_t flag) {
  aflag_del2(&socket->flags, flag);
}

void tcp_socket_cork_on(struct tcp_socket* const socket) {
  (void) net_socket_setopt_true(socket, tcp_protocol, TCP_CORK);
}

void tcp_socket_cork_off(struct tcp_socket* const socket) {
  (void) net_socket_setopt_false(socket, tcp_protocol, TCP_CORK);
}

void tcp_socket_nodelay_on(struct tcp_socket* const socket) {
  (void) net_socket_setopt_true(socket, tcp_protocol, TCP_NODELAY);
}

void tcp_socket_nodelay_off(struct tcp_socket* const socket) {
  (void) net_socket_setopt_false(socket, tcp_protocol, TCP_NODELAY);
}

static inline int tcp_socket_keepalive_internal(const struct tcp_socket* const socket, const int retries, const int idle_time, const int reprobe_time) {
  if(net_socket_setopt_true(socket, SOL_SOCKET, SO_KEEPALIVE) != 0) {
    return -1;
  }
  if(setsockopt(socket->net.sfd, IPPROTO_TCP, TCP_KEEPCNT, &retries, sizeof(int)) != 0) {
    return -1;
  }
  if(setsockopt(socket->net.sfd, IPPROTO_TCP, TCP_KEEPIDLE, &idle_time, sizeof(int)) != 0) {
    return -1;
  }
  if(setsockopt(socket->net.sfd, IPPROTO_TCP, TCP_KEEPINTVL, &reprobe_time, sizeof(int)) != 0) {
    return -1;
  }
  return 0;
}

int tcp_socket_keepalive(const struct tcp_socket* const socket) {
  return tcp_socket_keepalive_internal(socket, 10, 1, 1);
}

int tcp_socket_keepalive_explicit(const struct tcp_socket* const socket, const int retries, const int idle_time, const int reprobe_time) {
  return tcp_socket_keepalive_internal(socket, retries, idle_time, reprobe_time);
}

void tcp_socket_dont_receive_data(struct tcp_socket* const socket) {
  (void) shutdown(socket->net.sfd, SHUT_RD);
}



static void tcp_free_send_queue(struct tcp_socket* const socket) {
  if(socket->send_queue != NULL) {
    for(uint32_t i = 0; i < socket->send_len; ++i) {
      if(!socket->send_queue[i].dont_free) {
        free((void*) socket->send_queue[i].data);
      }
    }
    free(socket->send_queue);
    socket->send_queue = NULL;
    socket->send_len = 0;
  }
}

/* This function MUST only be called from within the onclose() callback or after
it occurs, NOT BEFORE */

void tcp_socket_free(struct tcp_socket* const socket) {
  if(socket->server != NULL) {
    /* By design, the server is either closing, or it is not. It can't become
    closing in this function.
    Additionally, since the order update in net_epoll, socket deletions will
    always be handled before server shutdowns, so we are actually GUARANTEED
    here that the server isn't closing. That means we can simply ignore the
    check and don't need to check if there are 0 connections to call the
    onshutdown callback. */
    struct tcp_server* const server = socket->server;
    (void) pthread_mutex_lock(&server->lock);
    if(socket->callbacks->onfree != NULL) {
      socket->callbacks->onfree(socket);
    }
    if(socket->net.sfd != -1) {
      (void) close(socket->net.sfd);
    }
    (void) pthread_mutex_destroy(&socket->lock);
    tcp_free_send_queue(socket);
    /* Return the socket's index for reuse */
    const uint32_t index = ((uintptr_t) socket - (uintptr_t) server->sockets) / server->socket_size;
    server->freeidx[server->freeidx_used++] = index;
    (void) memset(socket, 0, server->socket_size);
    (void) pthread_mutex_unlock(&server->lock);
  } else {
    if(socket->callbacks->onfree != NULL) {
      socket->callbacks->onfree(socket);
    }
    if(socket->net.sfd != -1) {
      (void) close(socket->net.sfd);
    }
    (void) pthread_mutex_destroy(&socket->lock);
    tcp_free_send_queue(socket);
    if(socket->addr != NULL) {
      free(socket->addr);
    }
    if(socket->alloc_epoll) {
      socket->epoll->close = 1;
      socket->epoll->free = 1;
    }
    if(socket->alloc_addr) {
      net_free_address(socket->info);
    }
    if(socket->settings.free_on_free) {
      free(socket);
    } else {
      socket->net.sfd = -1;
      socket->info = NULL;
      socket->cur_info = NULL;
      aflag_clear2(&socket->flags);
      socket->alloc_epoll = 0;
      socket->alloc_addr = 0;
      socket->reprobed = 0;
      socket->reconnecting = 0;
    }
  }
}

/* Attempt to gracefully close the connection. If there is any pending data in
the send queue, send it out first. */

void tcp_socket_close(struct tcp_socket* const socket) {
  (void) pthread_mutex_lock(&socket->lock);
  if(socket->send_len == 0) {
    tcp_socket_set_flag(socket, tcp_shutdown_wr);
    tcp_socket_clear_flag(socket, tcp_can_send);
    (void) shutdown(socket->net.sfd, SHUT_WR);
    tcp_free_send_queue(socket);
  } else {
    tcp_socket_set_flag(socket, tcp_closing);
  }
  (void) pthread_mutex_unlock(&socket->lock);
}

/* Not gracefully, but quicker */

void tcp_socket_force_close(struct tcp_socket* const socket) {
  tcp_socket_set_flag(socket, tcp_shutdown_wr);
  tcp_socket_clear_flag(socket, tcp_can_send);
  (void) shutdown(socket->net.sfd, SHUT_RDWR);
  tcp_free_send_queue(socket);
}



static int tcp_socket_connect(struct tcp_socket* const sock) {
  while(1) {
    if(sock->net.sfd != -1) {
      if((sock->reconnecting && sock->settings.oncreation_when_reconnect) || (!sock->reconnecting && sock->callbacks->onfree != NULL)) {
        sock->callbacks->onfree(sock);
      }
      (void) close(sock->net.sfd);
      sock->net.sfd = -1;
    }
    while(1) {
      sock->net.sfd = socket(sock->cur_info->ai_family, stream_socktype, tcp_protocol);
      if(sock->net.sfd == -1) {
        if(errno == ENOMEM && sock->callbacks->onnomem(sock) == 0) {
          continue;
        }
        return -1;
      }
      break;
    }
    errno = 0;
    (void) net_socket_connect(sock, sock->cur_info);
    switch(errno) {
      case 0:
      case EINPROGRESS: {
        if(net_socket_default_options(sock) != 0) {
          goto err_sock;
        }
        if(((sock->reconnecting && sock->settings.oncreation_when_reconnect) || (!sock->reconnecting && sock->callbacks->oncreation != NULL)) && sock->callbacks->oncreation(sock) != 0) {
          goto err_sock;
        }
        while(1) {
          if(net_epoll_add(sock->epoll, sock, EPOLLET | EPOLLRDHUP | EPOLLIN | EPOLLOUT) != 0) {
            if(errno == ENOMEM && sock->callbacks->onnomem(sock) == 0) {
              continue;
            }
            goto err_creation;
          }
          break;
        }
        break;
      }
      case EALREADY:
      case EBADF:
      case EISCONN:
      case ENOBUFS:
      case ENOMEM: goto err_sock;
      case EPIPE: continue;
      default: {
        if(sock->cur_info->ai_next == NULL) {
          goto err_sock;
        }
        sock->cur_info = sock->cur_info->ai_next;
        continue;
      }
    }
    break;
  }
  return 0;
  
  err_creation:
  sock->callbacks->onfree(sock);
  err_sock:
  close(sock->net.sfd);
  sock->net.sfd = -1;
  return -1;
}

#define socket ((struct tcp_socket*) addr->data)

static void tcp_socket_async_connect(struct net_async_address* addr, struct addrinfo* info) {
  if(info == NULL) {
    socket->callbacks->onclose(socket);
  } else {
    if(socket->alloc_addr) {
      net_free_address(socket->info);
    } else {
      socket->alloc_addr = 1;
    }
    socket->info = info;
    socket->cur_info = socket->info;
    while(1) {
      if(tcp_socket_connect(socket) == -1) {
        if(errno == ENOMEM && socket->callbacks->onnomem(socket) == 0) {
          continue;
        }
        socket->callbacks->onclose(socket);
      }
      break;
    }
  }
  free(addr);
}

#undef socket

int tcp_socket(struct tcp_socket* const sock, const struct tcp_socket_options* const opt) {
  if(sock->settings.init == 0) {
    sock->settings = tcp_socket_settings;
  }
  sock->net.socket = 1;
  const int ret = pthread_mutex_init(&sock->lock, NULL);
  if(ret != 0) {
    errno = ret;
    return -1;
  }
  if(sock->epoll == NULL) {
    sock->epoll = calloc(1, sizeof(struct net_epoll));
    if(sock->epoll == NULL) {
      goto err_mutex;
    }
    if(tcp_epoll(sock->epoll) != 0) {
      free(sock->epoll);
      goto err_mutex;
    }
    if(net_epoll_start(sock->epoll) != 0) {
      net_epoll_free(sock->epoll);
      free(sock->epoll);
      goto err_mutex;
    }
    sock->alloc_epoll = 1;
  }
  if(opt != NULL && opt->hostname != NULL) {
    const size_t hostname_len = strlen(opt->hostname);
    const size_t port_len = strlen(opt->port);
    sock->addr = malloc(sizeof(struct tcp_address) + hostname_len + port_len + 2);
    if(sock->addr == NULL) {
      goto err_epoll;
    }
    char* const hostname = (char*)(sock->addr + 1);
    char* const port = (char*)(sock->addr + 1) + hostname_len + 1;
    (void) memcpy(hostname, opt->hostname, hostname_len);
    hostname[hostname_len] = 0;
    (void) memcpy(port, opt->port, port_len);
    port[port_len] = 0;
    sock->addr->hostname = hostname;
    sock->addr->port = port;
  }
  sock->net.sfd = -1;
  if(sock->info != NULL) {
    sock->cur_info = sock->info;
    if(tcp_socket_connect(sock) != 0) {
      goto err_addr;
    }
  } else {
    struct net_async_address* const async = malloc(sizeof(struct net_async_address) + sizeof(struct addrinfo));
    if(async == NULL) {
      goto err_addr;
    }
    async->hostname = sock->addr->hostname;
    async->port = sock->addr->port;
    async->data = sock;
    async->callback = tcp_socket_async_connect;
    struct addrinfo* const info = (struct addrinfo*)(async + 1);
    info->ai_family = opt->family;
    info->ai_socktype = stream_socktype;
    info->ai_protocol = tcp_protocol;
    info->ai_flags = opt->flags;
    async->hints = info;
    if(net_get_address_async(async) == -1) {
      free(async);
      goto err_addr;
    }
  }
  return 0;
  
  err_addr:
  if(sock->addr != NULL) {
    free(sock->addr);
  }
  err_epoll:
  if(sock->alloc_epoll) {
    net_epoll_stop(sock->epoll);
    net_epoll_free(sock->epoll);
    free(sock->epoll);
    sock->epoll = NULL;
    sock->alloc_epoll = 0;
  }
  err_mutex:
  (void) pthread_mutex_destroy(&sock->lock);
  return -1;
}

/* Returns 0 if sent everything, -1 otherwise, -2 if a fatal error occured */

static int tcp_send_buffered(struct tcp_socket* const socket) {
  (void) pthread_mutex_lock(&socket->lock);
  if(socket->send_len != 0) {
    while(1) {
      const ssize_t bytes = send(socket->net.sfd, socket->send_queue->data + socket->send_queue->offset, socket->send_queue->len - socket->send_queue->offset, MSG_NOSIGNAL);
      if(bytes == -1) {
        switch(errno) {
          case EINTR: continue;
          case EPIPE: {
            tcp_socket_set_flag(socket, tcp_shutdown_wr);
            if(!socket->settings.automatically_reconnect) {
              tcp_socket_clear_flag(socket, tcp_can_send);
              tcp_free_send_queue(socket);
              (void) pthread_mutex_unlock(&socket->lock);
              return -2;
            }
          }
          case EAGAIN: {
            tcp_socket_clear_flag(socket, tcp_can_send);
          }
          default: {
            (void) pthread_mutex_unlock(&socket->lock);
            return -1;
          }
          case ENOMEM: {
            if(socket->callbacks->onnomem(socket) == 0) {
              continue;
            }
            (void) pthread_mutex_unlock(&socket->lock);
            return -2;
          }
        }
      }
      socket->send_queue->offset += bytes;
      if(socket->send_queue->offset == socket->send_queue->len) {
        if(!socket->send_queue->dont_free) {
          free((void*) socket->send_queue->data);
        }
        --socket->send_len;
        (void) memmove(socket->send_queue, socket->send_queue + 1, sizeof(struct tcp_socket_send_frame) * socket->send_len);
        if(socket->send_len == 0) {
          break;
        }
      }
    }
  }
  (void) pthread_mutex_unlock(&socket->lock);
  if(tcp_socket_test_flag(socket, tcp_closing)) {
    tcp_socket_set_flag(socket, tcp_shutdown_wr);
    tcp_socket_clear_flag(socket, tcp_can_send);
    (void) shutdown(socket->net.sfd, SHUT_WR);
    tcp_free_send_queue(socket);
  }
  return 0;
}

int tcp_buffer(struct tcp_socket* const socket, const void* data, uint64_t size, uint64_t offset, const int flags) {
  (void) pthread_mutex_lock(&socket->lock);
  while(1) {
    struct tcp_socket_send_frame* const ptr = realloc(socket->send_queue, sizeof(struct tcp_socket_send_frame) * (socket->send_len + 1));
    if(ptr == NULL) {
      if(socket->callbacks->onnomem(socket) == 0) {
        continue;
      }
      goto err;
    }
    socket->send_queue = ptr;
    break;
  }
  if(!(flags & tcp_read_only)) {
    size -= offset;
    data = (char*) data + offset;
    offset = 0;
    void* data_ptr;
    while(1) {
      data_ptr = malloc(size);
      if(data_ptr == NULL) {
        if(socket->callbacks->onnomem(socket) == 0) {
          continue;
        }
        goto err;
      }
      break;
    }
    (void) memcpy(data_ptr, data, size);
    socket->send_queue[socket->send_len++] = (struct tcp_socket_send_frame) { data_ptr, offset, 0, size, 1 };
  } else {
    socket->send_queue[socket->send_len++] = (struct tcp_socket_send_frame) { data, offset, 1, size, flags >> 1 };
  }
  (void) pthread_mutex_unlock(&socket->lock);
  return 0;
  
  err:
  (void) pthread_mutex_unlock(&socket->lock);
  if(!(flags & tcp_dont_free)) {
    free((void*) data);
  }
  return -1;
}

/* tcp_send() returns 0 on success and -1 on failure. It might set errno to an
error code.

If errno is EPIPE, no data may be sent in the future and partial or no data was
sent. That may be because either we closed the channel, or the connection is
closed.

Most applications can ignore the return value and errno. */

int tcp_send(struct tcp_socket* const socket, const void* data, uint64_t size, const int flags) {
  if(!socket->settings.automatically_reconnect && tcp_socket_test_flag(socket, tcp_shutdown_wr)) {
    errno = EPIPE;
    goto err0;
  }
  const int err = tcp_send_buffered(socket);
  if(err == -2) {
    goto err1;
  }
  uint64_t offset = 0;
  if(err == 0 && tcp_socket_test_flag(socket, tcp_can_send)) {
    while(1) {
      const ssize_t bytes = send(socket->net.sfd, (char*) data + offset, size - offset, MSG_NOSIGNAL);
      if(bytes == -1) {
        switch(errno) {
          case EINTR: continue;
          case EPIPE: {
            tcp_socket_set_flag(socket, tcp_shutdown_wr);
            if(!socket->settings.automatically_reconnect) {
              tcp_socket_clear_flag(socket, tcp_can_send);
              tcp_free_send_queue(socket);
              goto err1;
            }
          }
          case EAGAIN: {
            tcp_socket_clear_flag(socket, tcp_can_send);
          }
          default: break;
          case ENOMEM: {
            if(socket->callbacks->onnomem(socket) == 0) {
              continue;
            }
            goto err1;
          }
        }
        break;
      }
      offset += bytes;
      if(offset == size) {
        errno = 0;
        goto err0;
      }
    }
  }
  return tcp_buffer(socket, data, size, offset, flags);
  
  err1:
  if(!(flags & tcp_dont_free)) {
    free((void*) data);
  }
  return -1;
  
  err0:
  if(!(flags & tcp_dont_free)) {
    free((void*) data);
  }
  return 0;
}

/* tcp_read() returns amount of bytes read. It might set errno to an error code.

If errno is EPIPE, there is no more data to be read (but data might have been
read).

The onreadclose() callback DOES NOT mean there is no more data to be read. It
only means that the peer won't send more data - we can close our channel to
gracefully close the TCP connection. Data might still be pending in the kernel
buffer, waiting to be collected by the application using tcp_read(). */

uint64_t tcp_read(struct tcp_socket* const socket, void* data, uint64_t size) {
  /* We MUST NOT check if the socket is closed or closing for reading, because
  there still might be data pending in the kernel buffer that we might want. */
  const uint64_t all = size;
  while(1) {
    const ssize_t bytes = recv(socket->net.sfd, data, size, 0);
    if(bytes == -1) {
      if(errno == EINTR) {
        continue;
      }
      /* Wait until the next epoll iteration. If the error was fatal, the
      connection will be closed with an EPOLLHUP. Otherwise we can probably
      ignore it.
      Yes, the application could be notified about the error, but it doesn't
      need  to check the errno whatsoever if the socket will be closed by epoll
      anyway. The application can just keep call tcp_send() and tcp_read()
      without worrying about the underlying socket's state. */
      break;
    } else if(bytes == 0) {
      /* End of data, let epoll deal with the socket */
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

#define socket ((struct tcp_socket*) net)

/* A socket may only be in 1 epoll instance at a time */

static void tcp_socket_onevent(int events, struct net_socket* net) {
  if(events & EPOLLOUT) {
    _debug("epollout", 1);
    if(!tcp_socket_test_flag(socket, tcp_opened)) {
      tcp_socket_set_flag(socket, tcp_opened);
      if(socket->callbacks->onopen != NULL) {
        socket->callbacks->onopen(socket);
      }
    } else if(socket->reconnecting) {
      tcp_socket_clear_flag(socket, ~tcp_opened);
      if(socket->settings.onopen_when_reconnect) {
        socket->callbacks->onopen(socket);
      }
      socket->reconnecting = 0;
      socket->reprobed = 0;
    }
  }
  if((events & EPOLLIN) && socket->callbacks->onmessage != NULL) {
    _debug("epollin", 1);
    socket->callbacks->onmessage(socket);
  }
  int code = 0;
  (void) getsockopt(socket->net.sfd, SOL_SOCKET, SO_ERROR, &code, &(socklen_t){sizeof(int)});
  if((events & EPOLLERR) || code != 0) {
    errno = 0;
    if(code == EPIPE) {
      goto epollhup;
    }
    _debug("epollerr", 1);
    if(socket->settings.automatically_reconnect) {
      if(!socket->reconnecting && socket->settings.onclose_when_reconnect) {
        errno = EAGAIN;
        socket->callbacks->onclose(socket);
      }
      errno = 0;
      /* The difference between this and EPOLLHUP is that we don't try the same
      address at first */
      while(socket->cur_info->ai_next != NULL) {
        socket->cur_info = socket->cur_info->ai_next;
        int err;
        do {
          err = tcp_socket_connect(socket);
        } while(err == -1 && errno == ENOMEM && socket->callbacks->onnomem(socket) == 0);
        if(err == 0) {
          socket->reconnecting = 1;
          return;
        }
        break;
      }
      if(socket->net.sfd != -1) { /* ai_next was NULL */
        (void) close(socket->net.sfd);
        socket->net.sfd = -1;
      }
      /* If we failed connecting to all addresses, first check if we already did
      a full cycle */
      if(!socket->reprobed) {
        errno = 0;
        struct net_async_address* const async = malloc(sizeof(struct net_async_address) + sizeof(struct addrinfo));
        if(async != NULL) {
          async->hostname = socket->addr->hostname;
          async->port = socket->addr->port;
          async->data = socket;
          async->callback = tcp_socket_async_connect;
          struct addrinfo* const info = (struct addrinfo*)(async + 1);
          info->ai_family = socket->info->ai_family;
          info->ai_socktype = stream_socktype;
          info->ai_protocol = tcp_protocol;
          info->ai_flags = socket->info->ai_flags;
          async->hints = info;
          socket->reprobed = 1;
          socket->reconnecting = 1;
          if(net_get_address_async(async) == -1) {
            socket->reprobed = 0;
            socket->reconnecting = 0;
            free(async);
          } else {
            return;
          }
        }
      } /* else don't fall into an infinite loop and just close the connection*/
    }
    if(errno == 0) {
      errno = code;
    }
    if(socket->settings.free_on_free) {
      socket->callbacks->onclose(socket);
    } else {
      socket->callbacks->onclose(socket);
      if(socket->settings.init == 1) {
        (void) net_epoll_remove(socket->epoll, socket);
      }
    }
    return;
  }
  if(events & EPOLLHUP) {
    epollhup:
    _debug("epollhup", 1);
    if(socket->settings.automatically_reconnect) {
      if(socket->settings.onclose_when_reconnect) {
        errno = EAGAIN;
        socket->callbacks->onclose(socket);
      }
      errno = 0;
      while(1) {
        int err;
        do {
          err = tcp_socket_connect(socket);
        } while(err == -1 && errno == ENOMEM && socket->callbacks->onnomem(socket) == 0);
        if(err == 0) {
          socket->reconnecting = 1;
          return;
        }
        if(socket->cur_info->ai_next != NULL) {
          socket->cur_info = socket->cur_info->ai_next;
          continue;
        }
        break;
      }
      /* If all addresses failed, get fresh ones */
      struct net_async_address* const async = malloc(sizeof(struct net_async_address) + sizeof(struct addrinfo));
      if(async != NULL) {
        async->hostname = socket->addr->hostname;
        async->port = socket->addr->port;
        async->data = socket;
        async->callback = tcp_socket_async_connect;
        struct addrinfo* const info = (struct addrinfo*)(async + 1);
        info->ai_family = socket->info->ai_family;
        info->ai_socktype = stream_socktype;
        info->ai_protocol = tcp_protocol;
        info->ai_flags = socket->info->ai_flags;
        async->hints = info;
        socket->reprobed = 1;
        socket->reconnecting = 1;
        if(net_get_address_async(async) == -1) {
          socket->reprobed = 0;
          socket->reconnecting = 0;
          free(async);
        } else {
          return;
        }
      }
    } else {
      errno = 0;
    }
    if(socket->settings.free_on_free) {
      socket->callbacks->onclose(socket);
    } else {
      socket->callbacks->onclose(socket);
      if(socket->settings.init == 1) {
        /* The application didn't call onfree() */
        (void) net_epoll_remove(socket->epoll, socket);
      }
    }
    return;
  }
  if(events & EPOLLOUT) {
    tcp_socket_set_flag(socket, tcp_can_send);
    if(socket->callbacks->onsend != NULL) {
      socket->callbacks->onsend(socket);
    }
    if(!socket->settings.dont_send_buffered && tcp_socket_test_flag(socket, tcp_can_send)) {
      (void) tcp_send_buffered(socket);
    }
  }
  if(events & EPOLLRDHUP) {
    if(socket->callbacks->onreadclose != NULL) {
      socket->callbacks->onreadclose(socket);
    }
    if(socket->settings.automatically_close_onreadclose == 1) {
      tcp_socket_close(socket);
    }
  }
}

#undef socket



void tcp_server_free(struct tcp_server* const server) {
  (void) close(server->net.net.sfd);
  (void) pthread_mutex_destroy(&server->lock);
  if(server->alloc_sockets) {
    free(server->sockets);
    server->sockets = NULL;
    server->sockets_used = 0;
    server->alloc_sockets = 0;
  }
  if(server->alloc_freeidx) {
    free(server->freeidx);
    server->freeidx = NULL;
    server->freeidx_used = 0;
    server->alloc_freeidx = 0;
  }
  if(server->alloc_epoll) {
    server->epoll->close = 1;
    server->epoll->free = 1;
    server->epoll = NULL;
    server->alloc_epoll = 0;
  }
  server->disallow_connections = 0;
  server->socket_size = 0;
}

static void tcp_server_shutdown_socket_close(struct tcp_socket* socket, void* data) {
  (void) data;
  if(socket->callbacks->onfree != NULL) {
    socket->callbacks->onfree(socket);
  }
  (void) close(socket->net.sfd);
  (void) pthread_mutex_destroy(&socket->lock);
  tcp_free_send_queue(socket);
}

#define server ((struct tcp_server*) net)

static void tcp_server_shutdown_internal(void* net) {
  tcp_server_foreach_conn(server, tcp_server_shutdown_socket_close, NULL);
  server->callbacks->onshutdown(server);
}

#undef server

int tcp_server(struct tcp_server* const server, const struct tcp_server_options* const opt) {
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
    server->alloc_sockets = 1;
  }
  if(server->freeidx == NULL) {
    server->freeidx = malloc(sizeof(uint32_t) * server->settings->max_conn);
    if(server->freeidx == NULL) {
      goto err_sockets;
    }
    server->alloc_freeidx = 1;
  }
  const int err = pthread_mutex_init(&server->lock, NULL);
  if(err != 0) {
    errno = err;
    goto err_freeidx;
  }
  struct addrinfo* base_info;
  struct addrinfo* cur_info;
  uint8_t alloc_info = 0;
  if(opt->info == NULL) {
    const struct addrinfo hints = net_get_addr_struct(opt->family, stream_socktype, tcp_protocol, opt->flags);
    cur_info = net_get_address(opt->hostname, opt->port, &hints);
    if(cur_info == NULL) {
      goto err_mutex;
    }
    base_info = cur_info;
    alloc_info = 1;
  } else {
    cur_info = opt->info;
  }
  server->net.net.sfd = socket(cur_info->ai_family, stream_socktype, tcp_protocol);
  if(server->net.net.sfd == -1) {
    goto err_addr;
  }
  server->net.on_event = tcp_server_shutdown_internal;
  if(net_socket_default_options(server) != 0) {
    goto err_server;
  }
  if(server->epoll == NULL) {
    server->epoll = calloc(1, sizeof(struct net_epoll));
    if(server->epoll == NULL) {
      goto err_server;
    }
    if(tcp_epoll(server->epoll) != 0) {
      free(server->epoll);
      goto err_server;
    }
    if(net_epoll_start(server->epoll) != 0) {
      net_epoll_free(server->epoll);
      free(server->epoll);
      goto err_server;
    }
    server->alloc_epoll = 1;
  }
  while(1) {
    if(net_socket_bind(server, cur_info) != 0 || listen(server->net.net.sfd, server->settings->backlog) != 0) {
      if(cur_info->ai_next == NULL) {
        goto err_epoll;
      }
      cur_info = cur_info->ai_next;
    } else {
      break;
    }
  }
  if(net_epoll_add(server->epoll, server, EPOLLET | EPOLLIN) != 0) {
    goto err_epoll;
  }
  if(alloc_info) {
    net_free_address(base_info);
  }
  return 0;
  
  err_epoll:
  if(server->alloc_epoll) {
    net_epoll_stop(server->epoll);
    net_epoll_free(server->epoll);
    free(server->epoll);
    server->epoll = NULL;
    server->alloc_epoll = 0;
  }
  err_server:
  if(server->net.net.sfd != -1) {
    (void) close(server->net.net.sfd);
  }
  err_addr:
  if(alloc_info) {
    net_free_address(base_info);
  }
  err_mutex:
  (void) pthread_mutex_destroy(&server->lock);
  err_freeidx:
  if(server->alloc_freeidx) {
    free(server->freeidx);
    server->freeidx = NULL;
    server->alloc_freeidx = 0;
  }
  err_sockets:
  if(server->alloc_sockets) {
    free(server->sockets);
    server->sockets = NULL;
    server->alloc_sockets = 0;
  }
  return -1;
}

#define _server ((struct tcp_server*) net)

/* Throughput can be increased by creating multiple servers listening on the
same port to let the kernel load-balance. For larger amount of servers, own load
balancer might be needed. */

void tcp_server_onevent(int events, struct net_socket* net) {
  /* If we got an event, it will for sure be a new connection. We employ a while
  loop to accept until we hit EAGAIN. That way, we won't cause accidental
  starvation. */
  while(1) {
    if(aflag_test2(&_server->flags, tcp_closing)) {
      break;
    }
    struct sockaddr_in6 addr;
    socklen_t addrlen = sizeof(struct sockaddr_in6);
    const int sfd = accept(_server->net.net.sfd, (struct sockaddr*)&addr, &addrlen);
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
      } else if(_server->callbacks->onerror != NULL) {
        /* Optionally report the problem to the application and move on */
        _server->callbacks->onerror(_server);
      }
      continue;
    }
    /* First check if we hit the connection limit for the server, or if we don't
    accept new connections. If yes, just close the socket. This way, it is removed
    from the queue of pending connections and it won't get timed out, wasting system
    resources. */
    (void) pthread_mutex_lock(&_server->lock);
    if(_server->settings->max_conn == tcp_server_get_conn_amount_raw(_server) || _server->disallow_connections == 1) {
      goto err_sock;
    }
    /* Otherwise, get memory for the socket */
    struct tcp_socket* socket;
    /* First check if there are any free indexes. If not, we will just go with
    sockets_used. Note that we don't modify anything here. That is to not have
    to return the socket index if anything fails later on. We have the mutex
    for ourselves for the whole duration of the socket's creation, so might as
    well use it. */
    if(_server->freeidx_used == 0) {
      socket = (struct tcp_socket*)(_server->sockets + _server->sockets_used * _server->socket_size);
    } else {
      socket = (struct tcp_socket*)(_server->sockets + _server->freeidx[_server->freeidx_used - 1] * _server->socket_size);
    }
    /* Now we proceed to initialise required members of the socket. We don't
    have to zero it - it should already be zeroed. */
    socket->net.sfd = sfd;
    socket->net.socket = 1;
    socket->server = _server;
    socket->epoll = _server->epoll;
    if(_server->callbacks->onconnection(socket, (struct sockaddr*)&addr) != 0) {
      goto err_mem;
    }
    if(socket->settings.init == 0) {
      socket->settings = tcp_socket_settings;
    }
    while(1) {
      if(net_socket_default_options(socket) != 0) {
        if(errno == ENOMEM && _server->callbacks->onnomem(_server) == 0) {
          continue;
        }
        goto err_cb;
      }
      break;
    }
    while(1) {
      const int err = pthread_mutex_init(&socket->lock, NULL);
      if(err != 0) {
        if(err == ENOMEM && _server->callbacks->onnomem(_server) == 0) {
          continue;
        }
        errno = err;
        goto err_cb;
      }
      break;
    }
    while(1) {
      if(net_epoll_add(socket->epoll, socket, EPOLLET | EPOLLRDHUP | EPOLLIN | EPOLLOUT) != 0) {
        if(errno == ENOMEM && _server->callbacks->onnomem(_server) == 0) {
          continue;
        }
        (void) pthread_mutex_destroy(&socket->lock);
        goto err_cb;
      }
      break;
    }
    /* At this point, the socket is fully initialised */
    if(_server->freeidx_used == 0) {
      ++_server->sockets_used;
    } else {
      --_server->freeidx_used;
    }
    (void) pthread_mutex_unlock(&_server->lock);
    continue;
    
    err_cb:
    if(socket->callbacks->onfree != NULL) {
      socket->callbacks->onfree(socket);
    }
    if(errno != ENOMEM && _server->callbacks->onerror != NULL) {
      _server->callbacks->onerror(_server);
    }
    err_mem:
    (void) memset(socket, 0, _server->socket_size);
    err_sock:
    (void) pthread_mutex_unlock(&_server->lock);
    (void) close(sfd);
  }
}

#undef _server

#define socket ((struct tcp_socket*)(server->sockets + i * server->socket_size))

/* We allow the application to access the connections, but we will do it by
ourselves so that the application doesn't mess up somewhere. Note that order in
which the sockets are accessed is not linear.
If the application wishes to do something send_queue-related, it MUST first
acquire the socket's mutex by doing pthread_mutex_lock(&socket->lock), otherwise
the behavior is undefined.
The behavior is undefined if the callback function will attempt to shutdown or
free the server.
The thread will deadlock if the callback function tries calling
tcp_server_get_conn_amount(server);
It should instead call
tcp_server_get_conn_amount_raw(server); */

void tcp_server_foreach_conn(struct tcp_server* const server, void (*callback)(struct tcp_socket*, void*), void* data) {
  (void) pthread_mutex_lock(&server->lock);
  uint32_t amount = tcp_server_get_conn_amount_raw(server);
  if(amount != 0) {
    for(uint32_t i = 0; i < server->settings->max_conn; ++i) {
      if(socket->settings.init != 0) {
        callback(socket, data);
        if(--amount == 0) {
          break;
        }
      }
    }
  }
  (void) pthread_mutex_unlock(&server->lock);
}

#undef socket

void tcp_server_dont_accept_conn(struct tcp_server* const server) {
  (void) pthread_mutex_lock(&server->lock);
  server->disallow_connections = 1;
  (void) pthread_mutex_unlock(&server->lock);
}

void tcp_server_accept_conn(struct tcp_server* const server) {
  (void) pthread_mutex_lock(&server->lock);
  server->disallow_connections = 0;
  (void) pthread_mutex_unlock(&server->lock);
}

/* Asynchronously shutdown the server - remove it from it's epoll, close all of
it's sockets. If there are still any connected sockets bound to this server,
they will be silently closed (without onclose(), but with onfree()) and their
resources will be freed. Thus, the application MUST NOT do any kind of
operations on the sockets or the server in the meantime. */

int tcp_server_shutdown(struct tcp_server* const server) {
  tcp_server_dont_accept_conn(server);
  aflag_add2(&server->flags, tcp_closing);
  return net_epoll_create_event(server->epoll, server);
}

uint32_t tcp_server_get_conn_amount_raw(const struct tcp_server* const server) {
  return server->sockets_used - server->freeidx_used;
}

uint32_t tcp_server_get_conn_amount(struct tcp_server* const server) {
  (void) pthread_mutex_lock(&server->lock);
  const uint32_t connections = tcp_server_get_conn_amount_raw(server);
  (void) pthread_mutex_unlock(&server->lock);
  return connections;
}

static void tcp_onevent(struct net_epoll* epoll, int events, struct net_socket* net) {
  if(net->socket) {
    tcp_socket_onevent(events, net);
  } else {
    tcp_server_onevent(events, net);
  }
}

int tcp_epoll(struct net_epoll* const epoll) {
  epoll->on_event = tcp_onevent;
  return net_epoll(epoll, 1);
}