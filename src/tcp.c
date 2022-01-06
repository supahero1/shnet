#include "tcp.h"
#include "error.h"
#include "aflags.h"

#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <netinet/tcp.h>

static struct tcp_socket_settings tcp_socket_settings = { 1, 0, 0, 0, 0, 1, 0 };

static struct tcp_server_settings tcp_server_settings = { 32, 32 };

static void tcp_socket_set_flag(struct tcp_socket* const socket, const uint8_t flag) {
  aflag_add2(&socket->flags, flag);
}

static uint8_t tcp_socket_test_flag(const struct tcp_socket* const socket, const uint8_t flag) {
  return aflag_test2(&socket->flags, flag);
}

static void tcp_socket_clear_flag(struct tcp_socket* const socket, const uint8_t flag) {
  aflag_del2(&socket->flags, flag);
}

static void tcp_socket_clear_flags(struct tcp_socket* const socket) {
  aflag_clear2(&socket->flags);
}

void tcp_socket_cork_on(struct tcp_socket* const socket) {
  (void) net_socket_setopt_true(&socket->net, tcp_protocol, TCP_CORK);
}

void tcp_socket_cork_off(struct tcp_socket* const socket) {
  (void) net_socket_setopt_false(&socket->net, tcp_protocol, TCP_CORK);
}

void tcp_socket_nodelay_on(struct tcp_socket* const socket) {
  (void) net_socket_setopt_true(&socket->net, tcp_protocol, TCP_NODELAY);
}

void tcp_socket_nodelay_off(struct tcp_socket* const socket) {
  (void) net_socket_setopt_false(&socket->net, tcp_protocol, TCP_NODELAY);
}

void tcp_socket_keepalive_explicit(const struct tcp_socket* const socket, const int retries, const int idle_time, const int reprobe_time) {
  (void) setsockopt(socket->net.sfd, tcp_protocol, TCP_KEEPCNT, &retries, sizeof(int));
  (void) setsockopt(socket->net.sfd, tcp_protocol, TCP_KEEPIDLE, &idle_time, sizeof(int));
  (void) setsockopt(socket->net.sfd, tcp_protocol, TCP_KEEPINTVL, &reprobe_time, sizeof(int));
  (void) net_socket_setopt_true(&socket->net, SOL_SOCKET, SO_KEEPALIVE);
}

void tcp_socket_keepalive(const struct tcp_socket* const socket) {
  tcp_socket_keepalive_explicit(socket, 10, 1, 1);
}

void tcp_socket_dont_receive_data(struct tcp_socket* const socket) {
  (void) shutdown(socket->net.sfd, SHUT_RD);
}



static void tcp_socket_free_raw(struct tcp_socket* const socket) {
  if(socket->server != NULL) {
    struct tcp_server* const server = socket->server;
    (void) pthread_mutex_lock(&server->lock);
    if(socket->net.sfd != -1) {
      if(socket->on_event != NULL) {
        (void) socket->on_event(socket, tcp_destruction);
      }
      (void) close(socket->net.sfd);
    }
    (void) pthread_mutex_destroy(&socket->lock);
    data_storage_free(&socket->queue);
    /* Return the socket's index for reuse */
    *((uint32_t*) socket) = server->freeidx;
    server->freeidx = ((uintptr_t) socket - (uintptr_t) server->sockets) / server->socket_size;
    --server->sockets_len;
    (void) memset((char*) socket + 4, 0, server->socket_size - 4);
    (void) pthread_mutex_unlock(&server->lock);
  } else {
    if(socket->net.sfd != -1) {
      if(socket->on_event != NULL) {
        (void) socket->on_event(socket, tcp_destruction);
      }
      (void) close(socket->net.sfd);
      socket->net.sfd = -1;
    }
    (void) pthread_mutex_destroy(&socket->lock);
    data_storage_free(&socket->queue);
    struct net_epoll* epoll = NULL;
    if(socket->alloc_epoll) {
      epoll = socket->epoll;
      socket->epoll = NULL;
      socket->alloc_epoll = 0;
    }
    if(socket->alloc_info) {
      net_free_address(socket->info);
      socket->info = NULL;
      socket->alloc_info = 0;
    }
    if(socket->alloc_addr) {
      free(socket->addr);
      socket->addr = NULL;
      socket->alloc_addr = 0;
    }
    tcp_socket_clear_flags(socket);
    socket->opened = 0;
    socket->reprobed = 0;
    socket->reconnecting = 0;
    socket->confirm_free = 0;
    socket->settings.init = 0;
    if(socket->on_event != NULL) {
      (void) socket->on_event(socket, tcp_free);
    }
    if(epoll != NULL) {
      thread_cancellation_disable();
      net_epoll_stop(epoll);
      net_epoll_free(epoll);
      free(epoll);
      thread_cancellation_enable();
    }
  }
}

/* tcp_socket_free() can be called before onclose(), but it won't be executed
instantly. Instead, it will wait for onclose() and only then free the socket.
This scheduling-fashion of freeing can be combined with the graceful shutdown.
One can first call tcp_socket_close() and then tcp_socket_free() from a user
thread to asynchronously close and destroy a socket. To release any resources
associated with the socket after it's been destroyed, like its memory, one can
listen for the tcp_free event. Note that this can't be done with tcp_destruction,
because the socket is still being accessed after that event. tcp_free is the
last event to be reported and the socket is officially dead after it occurs. */

void tcp_socket_free(struct tcp_socket* const socket) {
  (void) pthread_mutex_lock(&socket->lock);
  if(!socket->confirm_free) {
    socket->confirm_free = 1;
    (void) pthread_mutex_unlock(&socket->lock);
    return;
  }
  (void) pthread_mutex_unlock(&socket->lock);
  tcp_socket_free_raw(socket);
}

static void tcp_socket_free_internal(struct tcp_socket* const socket) {
  if(socket->on_event != NULL) {
    if(errno == EAGAIN) {
      errno = 0;
    }
    (void) socket->on_event(socket, tcp_close);
    (void) pthread_mutex_lock(&socket->lock);
    if(!socket->confirm_free) {
      (void) pthread_mutex_unlock(&socket->lock);
      if(socket->settings.init == 1) {
        /* It's fine if the application doesn't instantly call tcp_socket_free(),
        but to prevent double close and other weird stuff, we need to remove it
        from its epoll. */
        (void) net_epoll_remove(socket->epoll, socket);
      }
    } else {
      (void) pthread_mutex_unlock(&socket->lock);
    }
  } else {
    (void) net_epoll_remove(socket->epoll, socket);
  }
  tcp_socket_free(socket);
}

/* Attempt to gracefully close the connection. If there is any pending data in
the send queue, send it out first. Overrides auto-reconnecting. */

static void tcp_socket_close_raw(struct tcp_socket* const socket) {
  tcp_socket_set_flag(socket, tcp_closing);
  if(data_storage_is_empty(&socket->queue)) {
    (void) shutdown(socket->net.sfd, SHUT_WR);
    tcp_socket_set_flag(socket, tcp_shutdown_wr);
    tcp_socket_clear_flag(socket, tcp_send_available);
    data_storage_free(&socket->queue);
  }
}

void tcp_socket_close(struct tcp_socket* const socket) {
  tcp_socket_set_flag(socket, tcp_closing);
  (void) pthread_mutex_lock(&socket->lock);
  if(socket->net.sfd != -1) {
    if(data_storage_is_empty(&socket->queue)) {
      (void) shutdown(socket->net.sfd, SHUT_WR);
      tcp_socket_set_flag(socket, tcp_shutdown_wr);
      tcp_socket_clear_flag(socket, tcp_send_available);
      data_storage_free(&socket->queue);
    }
  } else {
    data_storage_free(&socket->queue);
  }
  (void) pthread_mutex_unlock(&socket->lock);
}

/* Not gracefully, but quicker */

void tcp_socket_force_close(struct tcp_socket* const socket) {
  (void) pthread_mutex_lock(&socket->lock);
  tcp_socket_set_flag(socket, tcp_closing | tcp_shutdown_wr);
  if(socket->net.sfd != -1) {
    (void) shutdown(socket->net.sfd, SHUT_RDWR);
  }
  data_storage_free(&socket->queue);
  (void) pthread_mutex_unlock(&socket->lock);
}



static int tcp_socket_connect(struct tcp_socket* const sock) {
  int failure = 0;
  while(1) {
    (void) pthread_mutex_lock(&sock->lock);
    if(sock->net.sfd != -1) {
      if((sock->reconnecting && sock->settings.oncreation_when_reconnect) || (!sock->reconnecting && sock->on_event != NULL)) {
        (void) sock->on_event(sock, tcp_destruction);
      }
      (void) close(sock->net.sfd);
      sock->net.sfd = -1;
    }
    if(sock->cur_info == NULL || failure || tcp_socket_test_flag(sock, tcp_closing)) {
      (void) pthread_mutex_unlock(&sock->lock);
      return -1;
    }
    sock->net.sfd = net_socket_get(sock->cur_info);
    if(sock->net.sfd == -1) {
      /* There are no realistic errors that can occur here */
      (void) pthread_mutex_unlock(&sock->lock);
      return -1;
    }
    net_socket_default_options(&sock->net);
    if(((sock->reconnecting && sock->settings.oncreation_when_reconnect) || (!sock->reconnecting && sock->on_event != NULL)) && sock->on_event(sock, tcp_creation)) {
      (void) close(sock->net.sfd);
      sock->net.sfd = -1;
      (void) pthread_mutex_unlock(&sock->lock);
      return -1;
    }
    (void) pthread_mutex_unlock(&sock->lock);
    errno = 0;
    /* Obviously, the socket can't be locked for the whole connecting process.
    We only lock it to sync the socket's sfd. Additionally, it's worth being
    locked during oncreation and onfree, in case the callbacks are relying on
    it. After the lock above, simple shutdown() will close the socket.
    Otherwise, if a DNS lookup is being done so that the sfd is -1, an atomic
    flag will be set and discovered upon next connection attempt. */
    (void) net_socket_connect(&sock->net, sock->cur_info);
    switch(errno) {
      case 0:
      case EINPROGRESS: {
        if(net_epoll_add(sock->epoll, sock, EPOLLET | EPOLLRDHUP | EPOLLIN | EPOLLOUT) == -1) {
          failure = 1;
          continue;
        }
        return 0;
      }
      /* Trying again won't fix these errors */
      case ENOMEM:
      case ENOBUFS: {
        failure = 1;
        continue;
      }
      /* EPIPE may occur when the connection is closed during handshake. Since
      we managed to connect, the address is valid, so just retry. */
      case EPIPE: continue;
      /* Check new address. If we run out of them, the error is clearly fatal.*/
      default: {
        sock->cur_info = sock->cur_info->ai_next;
        continue;
      }
    }
  }
}

#define socket ((struct tcp_socket*) addr->data)

static void tcp_socket_async_connect(struct net_async_address* addr, struct addrinfo* info) {
  if(info == NULL) {
    tcp_socket_free_internal(socket);
  } else {
    if(socket->alloc_info) {
      net_free_address(socket->info);
    } else {
      socket->alloc_info = 1;
    }
    socket->info = info;
    socket->cur_info = info;
    if(tcp_socket_connect(socket) == -1) {
      tcp_socket_free_internal(socket);
    }
  }
  free(addr);
}

#undef socket

int tcp_socket(struct tcp_socket* const sock, const struct tcp_socket_options* const opt) {
  if(!sock->settings.init) {
    sock->settings = tcp_socket_settings;
  } else if((sock->settings.onopen_when_reconnect || sock->settings.onclose_when_reconnect || sock->settings.oncreation_when_reconnect) && sock->on_event == NULL) {
    errno = EINVAL;
    return -1;
  }
  sock->net.sfd = -1;
  sock->net.socket = 1;
  {
    int err;
    safe_execute(err = pthread_mutex_init(&sock->lock, NULL), err != 0, err);
    if(err != 0) {
      errno = err;
      return -1;
    }
  }
  if(sock->epoll == NULL) {
    safe_execute(sock->epoll = calloc(1, sizeof(struct net_epoll)), sock->epoll == NULL, ENOMEM);
    if(sock->epoll == NULL) {
      goto err_mutex;
    }
    if(tcp_socket_epoll(sock->epoll) == -1) {
      free(sock->epoll);
      sock->epoll = NULL;
      goto err_mutex;
    }
    if(net_epoll_start(sock->epoll) == -1) {
      net_epoll_free(sock->epoll);
      free(sock->epoll);
      sock->epoll = NULL;
      goto err_mutex;
    }
    sock->alloc_epoll = 1;
  }
  if(sock->addr == NULL && opt != NULL && opt->hostname != NULL && opt->port != NULL) {
    const size_t hostname_len = strlen(opt->hostname);
    const size_t port_len = strlen(opt->port);
    safe_execute(sock->addr = malloc(sizeof(struct tcp_address) + (opt->static_hostname ? 0 : hostname_len + 1) + (opt->static_port ? 0 : port_len + 1)), sock->addr == NULL, ENOMEM);
    if(sock->addr == NULL) {
      goto err_epoll;
    }
    sock->alloc_addr = 1;
    if(opt->static_hostname) {
      if(opt->static_port) {
        sock->addr->hostname = opt->hostname;
        sock->addr->port = opt->port;
      } else {
        sock->addr->hostname = opt->hostname;
        
        char* const port = (char*)(sock->addr + 1);
        (void) memcpy(port, opt->port, port_len);
        port[port_len] = 0;
        sock->addr->port = port;
      }
    } else {
      if(opt->static_port) {
        sock->addr->port = opt->port;
        
        char* const hostname = (char*)(sock->addr + 1);
        (void) memcpy(hostname, opt->hostname, hostname_len);
        hostname[hostname_len] = 0;
        sock->addr->hostname = hostname;
      } else {
        char* const hostname = (char*)(sock->addr + 1);
        char* const port = (char*)(sock->addr + 1) + hostname_len + 1;
        (void) memcpy(hostname, opt->hostname, hostname_len);
        hostname[hostname_len] = 0;
        (void) memcpy(port, opt->port, port_len);
        port[port_len] = 0;
        sock->addr->hostname = hostname;
        sock->addr->port = port;
      }
    }
  }
  if(sock->addr == NULL && sock->settings.automatically_reconnect) {
    errno = EINVAL;
    return -1;
  }
  if(sock->info != NULL) {
    sock->cur_info = sock->info;
    if(tcp_socket_connect(sock) == -1) {
      goto err_addr;
    }
  } else {
    if(opt == NULL || opt->hostname == NULL || opt->port == NULL) {
      errno = EINVAL;
      return -1;
    }
    struct net_async_address* async;
    safe_execute(async = malloc(sizeof(struct net_async_address) + sizeof(struct addrinfo)), async == NULL, ENOMEM);
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
  if(sock->alloc_addr) {
    free(sock->addr);
    sock->addr = NULL;
    sock->alloc_addr = 0;
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
  if(!data_storage_is_empty(&socket->queue)) {
    while(1) {
      ssize_t bytes;
      safe_execute(bytes = send(socket->net.sfd, socket->queue.frames->data + socket->queue.frames->offset, socket->queue.frames->len - socket->queue.frames->offset, MSG_NOSIGNAL), bytes == -1, errno);
      if(bytes == -1) {
        switch(errno) {
          case EINTR: continue;
          case ECONNRESET:
          case EPIPE: {
            tcp_socket_set_flag(socket, tcp_shutdown_wr);
            if(!socket->settings.automatically_reconnect) {
              tcp_socket_clear_flag(socket, tcp_send_available);
              data_storage_free(&socket->queue);
              return -2;
            }
          }
          case EAGAIN: {
            tcp_socket_clear_flag(socket, tcp_send_available);
            data_storage_finish(&socket->queue);
          }
          default: return -1;
        }
      }
      if(data_storage_drain(&socket->queue, bytes)) {
        break;
      }
    }
  }
  if(tcp_socket_test_flag(socket, tcp_closing)) {
    (void) shutdown(socket->net.sfd, SHUT_WR);
    tcp_socket_clear_flag(socket, tcp_send_available);
    tcp_socket_set_flag(socket, tcp_shutdown_wr);
    data_storage_free(&socket->queue);
  }
  return 0;
}

int tcp_buffer(struct tcp_socket* const socket, void* const data, const uint64_t size, const uint64_t offset, const enum data_storage_flags flags) {
  (void) pthread_mutex_lock(&socket->lock);
  const int ret = data_storage_add(&socket->queue, data, size, offset, flags);
  (void) pthread_mutex_unlock(&socket->lock);
  return ret;
}

/* tcp_send() returns 0 on success and -1 on failure. It might set errno to an
error code.

If errno is EPIPE, no data may be sent in the future and partial or no data was
sent. That may be because either we closed the channel, or the connection is
closed.

Most applications can ignore the return value and errno. */

int tcp_send(struct tcp_socket* const socket, void* data, uint64_t size, const enum data_storage_flags flags) {
  if(!socket->settings.automatically_reconnect && tcp_socket_test_flag(socket, tcp_shutdown_wr)) {
    errno = EPIPE;
    goto err1;
  }
  (void) pthread_mutex_lock(&socket->lock);
  if(!tcp_socket_test_flag(socket, tcp_send_available)) {
    errno = 0;
    const int ret = data_storage_add(&socket->queue, data, size, 0, flags);
    (void) pthread_mutex_unlock(&socket->lock);
    return ret;
  }
  const int err = tcp_send_buffered(socket);
  if(err == -2) {
    (void) pthread_mutex_unlock(&socket->lock);
    goto err1;
  }
  if(err == -1) {
    errno = 0;
    const int ret = data_storage_add(&socket->queue, data, size, 0, flags);
    (void) pthread_mutex_unlock(&socket->lock);
    return ret;
  }
  uint64_t offset = 0;
  while(1) {
    ssize_t bytes;
    safe_execute(bytes = send(socket->net.sfd, (char*) data + offset, size - offset, MSG_NOSIGNAL), bytes == -1, errno);
    if(bytes == -1) {
      switch(errno) {
        case EINTR: continue;
        case ECONNRESET:
        case EPIPE: {
          tcp_socket_set_flag(socket, tcp_shutdown_wr);
          if(!socket->settings.automatically_reconnect) {
            tcp_socket_clear_flag(socket, tcp_send_available);
            data_storage_free(&socket->queue);
            (void) pthread_mutex_unlock(&socket->lock);
            goto err1;
          }
        }
        case EAGAIN: {
          tcp_socket_clear_flag(socket, tcp_send_available);
        }
        default: {
          const int ret = data_storage_add(&socket->queue, data, size, offset, flags);
          (void) pthread_mutex_unlock(&socket->lock);
          return ret;
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
  
  err1:
  if(!(flags & data_dont_free)) {
    free(data);
  }
  return -1;
  
  err0:
  (void) pthread_mutex_unlock(&socket->lock);
  if(!(flags & data_dont_free)) {
    free(data);
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
    ssize_t bytes;
    safe_execute(bytes = recv(socket->net.sfd, data, size, 0), bytes == -1, errno);
    if(bytes == -1) {
      if(errno == EINTR) {
        continue;
      }
      /* Wait for the next epoll iteration. If the error was fatal, the
      connection will be closed. Otherwise we can probably ignore it. */
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

static void tcp_reconnect(struct tcp_socket* socket, const int current_failed, const int code) {
  if(socket->settings.automatically_reconnect && !tcp_socket_test_flag(socket, tcp_closing)) {
    if(!socket->reconnecting && socket->settings.onclose_when_reconnect) {
      errno = EAGAIN;
      (void) socket->on_event(socket, tcp_close);
    }
    if(current_failed) {
      socket->cur_info = socket->cur_info->ai_next;
    }
    socket->reconnecting = 1;
    if(tcp_socket_connect(socket) == 0) {
      return;
    }
    /* If we failed connecting to all addresses, first check if we already did
    a full cycle */
    if(!socket->reprobed) {
      struct net_async_address* async;
      safe_execute(async = malloc(sizeof(struct net_async_address) + sizeof(struct addrinfo)), async == NULL, ENOMEM);
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
        if(net_get_address_async(async) == -1) {
          free(async);
        } else {
          return;
        }
      }
    } /* else don't fall into an infinite loop and just close the connection */
  }
  
  errno = code;
  tcp_socket_free_internal(socket);
}

#define socket ((struct tcp_socket*) net)

static void tcp_socket_onevent(uint32_t events, struct net_socket* net) {
  /* Multiple events stack on top of each other. To prevent weird flow like
  closing first and then opening (but the socket is closed, the order is wrong),
  we check for opening at the beginning, read any available data, check for
  socket closure, and only then can we begin sending (buffered) data.
  EDIT: EPOLLIN is received when getting ECONNREFUSED, so now connect() error
  checking is done first in order. The rest is unchanged. */
  int code = 0;
  (void) getsockopt(socket->net.sfd, SOL_SOCKET, SO_ERROR, &code, &(socklen_t){sizeof(int)});
  if(code == EPIPE || code == ECONNREFUSED) {
    tcp_reconnect(socket, 1, code);
    return;
  }
  if(events & EPOLLOUT) {
    if(!socket->opened) {
      socket->opened = 1;
      if(socket->on_event != NULL) {
        errno = 0;
        (void) socket->on_event(socket, tcp_open);
      }
    } else if(socket->reconnecting) {
      tcp_socket_clear_flag(socket, tcp_shutdown_wr | tcp_send_available);
      if(socket->settings.onopen_when_reconnect) {
        errno = EAGAIN;
        (void) socket->on_event(socket, tcp_open);
      }
      socket->reconnecting = 0;
      socket->reprobed = 0;
    }
  }
  if((events & EPOLLIN) && socket->on_event != NULL) {
    (void) socket->on_event(socket, tcp_data);
  }
  if(code != 0) {
    switch(code) {
      case ENETRESET:
      case ECONNRESET: goto epollhup;
    }
    tcp_reconnect(socket, 1, code);
    return;
  }
  if(events & EPOLLHUP) {
    epollhup:
    tcp_reconnect(socket, 0, code);
    return;
  }
  if(events & EPOLLOUT) {
    tcp_socket_set_flag(socket, tcp_send_available);
    if(socket->on_event != NULL) {
      (void) socket->on_event(socket, tcp_can_send);
    }
    if(!socket->settings.dont_send_buffered && tcp_socket_test_flag(socket, tcp_send_available)) {
      (void) pthread_mutex_lock(&socket->lock);
      (void) tcp_send_buffered(socket);
      (void) pthread_mutex_unlock(&socket->lock);
    }
  }
  if(events & EPOLLRDHUP) {
    if(socket->on_event != NULL) {
      (void) socket->on_event(socket, tcp_readclose);
    }
    if(socket->settings.automatically_close_onreadclose) {
      if(!socket->settings.automatically_reconnect) {
        tcp_socket_close_raw(socket);
      } else {
        (void) shutdown(socket->net.sfd, SHUT_WR);
        tcp_socket_set_flag(socket, tcp_shutdown_wr);
        tcp_socket_clear_flag(socket, tcp_send_available);
      }
    }
  }
}

#undef socket



static void tcp_server_set_flag(struct tcp_server* const server, const uint8_t flag) {
  aflag_add2(&server->flags, flag);
}

static uint8_t tcp_server_test_flag(const struct tcp_server* const server, const uint8_t flag) {
  return aflag_test2(&server->flags, flag);
}

void tcp_server_free(struct tcp_server* const server) {
  (void) server->on_event(server, tcp_destruction, NULL, NULL);
  (void) close(server->net.sfd);
  server->net.sfd = -1;
  (void) pthread_mutex_destroy(&server->lock);
  if(server->alloc_sockets) {
    free(server->sockets);
    server->sockets = NULL;
    server->alloc_sockets = 0;
  }
  struct net_epoll* epoll = NULL;
  if(server->alloc_epoll) {
    epoll = server->epoll;
    server->epoll = NULL;
    server->alloc_epoll = 0;
  }
  server->disallow_connections = 0;
  server->sockets_used = 0;
  server->sockets_len = 0;
  (void) server->on_event(server, tcp_free, NULL, NULL);
  if(epoll != NULL) {
    thread_cancellation_disable();
    net_epoll_stop(epoll);
    net_epoll_free(epoll);
    free(epoll);
    thread_cancellation_enable();
  }
}

static void tcp_server_shutdown_socket_close(struct tcp_socket* socket, void* nil) {
  if(socket->on_event != NULL) {
    (void) socket->on_event(socket, tcp_destruction);
  }
  (void) close(socket->net.sfd);
  (void) pthread_mutex_destroy(&socket->lock);
  data_storage_free(&socket->queue);
}

int tcp_server(struct tcp_server* const server, const struct tcp_server_options* const opt) {
  if(server->on_event == NULL) {
    errno = EINVAL;
    return -1;
  }
  server->freeidx = UINT32_MAX;
  if(server->settings == NULL) {
    server->settings = &tcp_server_settings;
  }
  if(server->socket_size == 0) {
    server->socket_size = sizeof(struct tcp_serversock);
  }
  if(server->sockets == NULL) {
    safe_execute(server->sockets = calloc(server->settings->max_conn, server->socket_size), server->sockets == NULL, ENOMEM);
    if(server->sockets == NULL) {
      return -1;
    }
    server->alloc_sockets = 1;
  }
  int err;
  safe_execute(err = pthread_mutex_init(&server->lock, NULL), err != 0, err);
  if(err != 0) {
    errno = err;
    goto err_sockets;
  }
  struct addrinfo* base_info;
  struct addrinfo* cur_info;
  uint8_t alloc_info = 0;
  if(opt->info == NULL) {
    const struct addrinfo hints = net_get_addr_struct(opt->family, stream_socktype, tcp_protocol, opt->flags | wants_a_server);
    cur_info = net_get_address(opt->hostname, opt->port, &hints);
    if(cur_info == NULL) {
      goto err_mutex;
    }
    base_info = cur_info;
    alloc_info = 1;
  } else {
    base_info = NULL;
    cur_info = opt->info;
  }
  if(server->epoll == NULL) {
    safe_execute(server->epoll = calloc(1, sizeof(struct net_epoll)), server->epoll == NULL, ENOMEM);
    if(server->epoll == NULL) {
      goto err_addr;
    }
    if(tcp_server_epoll(server->epoll) == -1) {
      free(server->epoll);
      goto err_addr;
    }
    if(net_epoll_start(server->epoll) == -1) {
      net_epoll_free(server->epoll);
      free(server->epoll);
      goto err_addr;
    }
    server->alloc_epoll = 1;
  }
  while(1) {
    server->net.sfd = net_socket_get(cur_info);
    if(server->net.sfd == -1) {
      goto err_epoll;
    }
    net_socket_default_options(&server->net);
    if(net_socket_bind(&server->net, cur_info) == -1 || listen(server->net.sfd, server->settings->backlog) == -1) {
      if(cur_info->ai_next == NULL) {
        goto err_server;
      }
      cur_info = cur_info->ai_next;
    } else {
      break;
    }
  }
  if(net_epoll_add(server->epoll, server, EPOLLET | EPOLLIN) == -1) {
    goto err_epoll;
  }
  if(alloc_info) {
    net_free_address(base_info);
  }
  return 0;
  
  err_server:
  (void) close(server->net.sfd);
  server->net.sfd = -1;
  err_epoll:
  if(server->alloc_epoll) {
    net_epoll_stop(server->epoll);
    net_epoll_free(server->epoll);
    free(server->epoll);
    server->epoll = NULL;
    server->alloc_epoll = 0;
  }
  err_addr:
  if(alloc_info) {
    net_free_address(base_info);
  }
  err_mutex:
  (void) pthread_mutex_destroy(&server->lock);
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
same port ON DIFFERENT CPU'S to let the kernel load-balance. Note that the
kernel load-balancer isn't perfect and will sometimes assign more or less to
some servers. Creating more servers than there are cores is meaningless. */

void tcp_server_onevent(uint32_t events, struct net_socket* net) {
  if(events == -1) {
    goto err_close;
  }
  /* A new event is guaranteed to be a new connection. We employ a while loop
  to accept until we hit EAGAIN to not cause accidental starvation. */
  while(1) {
    if(tcp_server_test_flag(_server, tcp_closing)) {
      return;
    }
    struct sockaddr_in6 addr;
    socklen_t addrlen = sizeof(struct sockaddr_in6);
    int sfd;
    safe_execute(sfd = accept(_server->net.sfd, (struct sockaddr*)&addr, &addrlen), sfd == -1, errno);
    if(sfd == -1) {
      if(errno == EAGAIN) {
        /* No more connections to accept */
        return;
      }
      continue;
    }
    /* First check if we hit the connection limit for the server, or if we don't
    accept new connections. If yes, just close the socket. This way, it is
    removed from the queue of pending connections and it won't get timed out,
    wasting system resources. */
    (void) pthread_mutex_lock(&_server->lock);
    if(_server->settings->max_conn == tcp_server_get_conn_amount_raw(_server) || _server->disallow_connections == 1) {
      goto err_sock;
    }
    /* Otherwise, get memory for the socket */
    struct tcp_socket* socket;
    /* First check if there are any free indexes. If not, we will just go with
    sockets_used. */
    if(_server->freeidx != UINT32_MAX) {
      socket = (struct tcp_socket*)(_server->sockets + _server->freeidx * _server->socket_size);
      _server->freeidx = *((uint32_t*) socket);
      *((uint32_t*) socket) = 0;
    } else {
      socket = (struct tcp_socket*)(_server->sockets + _server->sockets_used * _server->socket_size);
      ++_server->sockets_used;
    }
    /* Now we proceed to initialise required members of the socket. We don't
    have to zero it - it should already be zeroed. */
    socket->net.sfd = sfd;
    socket->net.socket = 1;
    socket->server = _server;
    if(_server->on_event(_server, tcp_creation, socket, (struct sockaddr*)&addr)) {
      goto err_mem;
    }
    if(socket->settings.init == 0) {
      socket->settings = tcp_socket_settings;
    }
    net_socket_default_options(&socket->net);
    int err;
    safe_execute(err = pthread_mutex_init(&socket->lock, NULL), err != 0, err);
    if(err != 0) {
      goto err_creation;
    }
    if(net_epoll_add(_server->epoll, socket, EPOLLET | EPOLLRDHUP | EPOLLIN | EPOLLOUT) == -1) {
      goto err_mutex;
    }
    /* At this point, the socket is fully initialised */
    ++_server->sockets_len;
    (void) pthread_mutex_unlock(&_server->lock);
    continue;
    
    err_mutex:
    (void) pthread_mutex_destroy(&socket->lock);
    err_creation:
    if(socket->on_event != NULL) {
      (void) socket->on_event(socket, tcp_destruction);
    }
    err_mem:
    (void) memset(socket, 0, _server->socket_size);
    *((uint32_t*) socket) = _server->freeidx;
    _server->freeidx = ((uintptr_t) socket - (uintptr_t) _server->sockets) / _server->socket_size;
    err_sock:
    (void) pthread_mutex_unlock(&_server->lock);
    (void) close(sfd);
  }
  
  err_close:
  tcp_server_foreach_conn(_server, tcp_server_shutdown_socket_close, NULL);
  (void) _server->on_event(_server, tcp_close, NULL, NULL);
}

#undef _server

#define socket ((struct tcp_socket*)(server->sockets + i * server->socket_size))

/* We allow the application to access the connections, but we will do it by
ourselves so that the application doesn't mess up somewhere. Note that there
may be gaps in the array of sockets.
If the application wishes to do something send_queue-related, it MUST first
acquire the socket's mutex by doing pthread_mutex_lock(&socket->lock), otherwise
the behavior is undefined.
The behavior is undefined if the callback function will attempt to shutdown or
free the server.
The thread will deadlock if the callback function tries calling
tcp_server_get_conn_amount(server);
It should instead call
tcp_server_get_conn_amount_raw(server);
The same is true for dont_accept_conn and accept_conn functions. */

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

void tcp_server_dont_accept_conn_raw(struct tcp_server* const server) {
  server->disallow_connections = 1;
}

void tcp_server_dont_accept_conn(struct tcp_server* const server) {
  (void) pthread_mutex_lock(&server->lock);
  tcp_server_dont_accept_conn_raw(server);
  (void) pthread_mutex_unlock(&server->lock);
}

void tcp_server_accept_conn_raw(struct tcp_server* const server) {
  server->disallow_connections = 0;
}

void tcp_server_accept_conn(struct tcp_server* const server) {
  (void) pthread_mutex_lock(&server->lock);
  tcp_server_accept_conn_raw(server);
  (void) pthread_mutex_unlock(&server->lock);
}

/* Asynchronously shutdown the server - remove it from it's epoll, close all of
it's sockets. If there are still any connected sockets bound to this server,
they will be closed with tcp_destruction event and their resources will be
freed. The application MUST NOT do any kind of operations on the sockets or the
server in the meantime. */

int tcp_server_shutdown(struct tcp_server* const server) {
  tcp_server_dont_accept_conn(server);
  tcp_server_set_flag(server, tcp_closing);
  return net_epoll_create_event(server->epoll, server);
}

uint32_t tcp_server_get_conn_amount_raw(const struct tcp_server* const server) {
  return server->sockets_len;
}

uint32_t tcp_server_get_conn_amount(struct tcp_server* const server) {
  (void) pthread_mutex_lock(&server->lock);
  const uint32_t connections = tcp_server_get_conn_amount_raw(server);
  (void) pthread_mutex_unlock(&server->lock);
  return connections;
}

static void tcp_onevent(struct net_epoll* epoll, uint32_t events, struct net_socket* net) {
  if(net->socket) {
    tcp_socket_onevent(events, net);
  } else {
    tcp_server_onevent(events, net);
  }
}

int tcp_socket_epoll(struct net_epoll* const epoll) {
  epoll->on_event = tcp_onevent;
  return net_socket_epoll(epoll);
}

int tcp_server_epoll(struct net_epoll* const epoll) {
  epoll->on_event = tcp_onevent;
  return net_server_epoll(epoll);
}