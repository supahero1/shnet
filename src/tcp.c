#include <errno.h>
#include <fcntl.h>
#include <assert.h>
#include <unistd.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <linux/tcp.h>
#include <stdatomic.h>
#include <sys/ioctl.h>
#include <sys/sendfile.h>

#include <shnet/tcp.h>
#include <shnet/error.h>

void tcp_lock(struct tcp_socket* const socket) {
  (void) pthread_mutex_lock(&socket->lock);
}

void tcp_unlock(struct tcp_socket* const socket) {
  (void) pthread_mutex_unlock(&socket->lock);
}

void tcp_socket_cork_on(const struct tcp_socket* const socket) {
  (void) net_socket_setopt_true(socket->core.fd, net_proto_tcp, TCP_CORK);
}

void tcp_socket_cork_off(const struct tcp_socket* const socket) {
  (void) net_socket_setopt_false(socket->core.fd, net_proto_tcp, TCP_CORK);
}

void tcp_socket_nodelay_on(const struct tcp_socket* const socket) {
  (void) net_socket_setopt_true(socket->core.fd, net_proto_tcp, TCP_NODELAY);
}

void tcp_socket_nodelay_off(const struct tcp_socket* const socket) {
  (void) net_socket_setopt_false(socket->core.fd, net_proto_tcp, TCP_NODELAY);
}

void tcp_socket_keepalive_on_explicit(const struct tcp_socket* const socket, const int idle_time, const int reprobe_time, const int retries) {
  (void) setsockopt(socket->core.fd, net_proto_tcp, TCP_KEEPIDLE, &idle_time, sizeof(int));
  (void) setsockopt(socket->core.fd, net_proto_tcp, TCP_KEEPINTVL, &reprobe_time, sizeof(int));
  (void) setsockopt(socket->core.fd, net_proto_tcp, TCP_KEEPCNT, &retries, sizeof(int));
  (void) setsockopt(socket->core.fd, net_proto_tcp, TCP_USER_TIMEOUT, (int[]){ (idle_time + reprobe_time * retries) * 1000 }, sizeof(int));
  (void) net_socket_setopt_true(socket->core.fd, SOL_SOCKET, SO_KEEPALIVE);
}

void tcp_socket_keepalive_on(const struct tcp_socket* const socket) {
  tcp_socket_keepalive_on_explicit(socket, 1, 1, 10);
}

void tcp_socket_keepalive_off(const struct tcp_socket* const socket) {
  (void) setsockopt(socket->core.fd, net_proto_tcp, TCP_USER_TIMEOUT, (int[]){ 0 }, sizeof(int));
  (void) net_socket_setopt_false(socket->core.fd, SOL_SOCKET, SO_KEEPALIVE);
}

void tcp_socket_free_(struct tcp_socket* const socket) {
  if(socket->on_event != NULL) {
    socket->on_event(socket, tcp_deinit);
  }
  if(socket->core.fd != -1) {
    (void) close(socket->core.fd);
    socket->core.fd = -1;
  }
  (void) pthread_mutex_destroy(&socket->lock);
  data_storage_free(&socket->queue);
  if(socket->alloc_loop) {
    async_loop_shutdown(socket->loop, async_free | async_ptr_free);
    socket->loop = NULL;
    socket->alloc_loop = 0;
  }
  socket->opened = 0;
  socket->confirmed_free = 0;
  socket->closing = 0;
  socket->close_guard = 0;
  socket->closing_fast = 0;
  uint8_t free_ = socket->free;
  socket->free = 0;
  if(socket->on_event != NULL) {
    socket->on_event(socket, tcp_free);
  }
  if(free_) {
    free(socket);
  }
}

void tcp_socket_free(struct tcp_socket* const socket) {
  tcp_lock(socket);
  if(socket->confirmed_free) {
    tcp_unlock(socket);
    tcp_socket_free_(socket);
  } else {
    socket->confirmed_free = 1;
    tcp_unlock(socket);
  }
}

static void tcp_socket_free_internal(struct tcp_socket* const socket) {
  if(socket->on_event != NULL) {
    socket->on_event(socket, tcp_close);
  }
  tcp_lock(socket);
  if(socket->confirmed_free) {
    tcp_unlock(socket);
    /*
     * Calling tcp_socket_free() twice is undefined behavior, so this is safe.
     */
    tcp_socket_free_(socket);
  } else {
    socket->confirmed_free = 1;
    (void) async_loop_remove(socket->loop, &socket->core);
    tcp_unlock(socket);
  }
}

void tcp_socket_close(struct tcp_socket* const socket) {
  tcp_lock(socket);
  socket->closing = 1;
  if(socket->opened && data_storage_is_empty(&socket->queue)) {
    socket->close_guard = 1;
    (void) shutdown(socket->core.fd, SHUT_WR);
    /*
     * You might think unlocking the socket after the syscall is a waste of
     * time, but in fact at this stage of a socket's lifetime, and especially
     * when this function can be freely called outside of the socket's event
     * handler, the socket may be freed right after tcp_unlock(), so the above
     * access to socket's core.fd will then result in undefined behavior.
     *
     * The same applies to tcp_socket_free_internal() above at
     * async_loop_remove(). Can't unlock before that.
     */
  }
  tcp_unlock(socket);
}

void tcp_socket_force_close(struct tcp_socket* const socket) {
  tcp_lock(socket);
  socket->closing_fast = 1;
  data_storage_free(&socket->queue);
  if(socket->opened) {
    socket->close_guard = 1;
    (void) shutdown(socket->core.fd, SHUT_RDWR);
  }
  tcp_unlock(socket);
}

static int tcp_socket_connect(struct tcp_socket* const socket, const struct addrinfo* info) {
  unsigned int ers = 0;
  while(1) {
    tcp_lock(socket);
    if(socket->core.fd != -1) {
      (void) close(socket->core.fd);
      socket->core.fd = -1;
    }
    if(info == NULL || socket->closing_fast || (socket->closing && data_storage_is_empty(&socket->queue))) {
      goto err;
    }
    socket->core.fd = net_socket_get(info);
    if(socket->core.fd == -1) {
      goto err;
    }
    net_socket_default_options(socket->core.fd);
    tcp_unlock(socket);
    errno = 0;
    (void) net_socket_connect(socket->core.fd, info);
    switch(errno) {
      case 0:
      case EINTR:
      case EINPROGRESS: {
        if(async_loop_add(socket->loop, &socket->core, EPOLLET | EPOLLRDHUP | EPOLLIN | EPOLLOUT) == -1) {
          info = NULL;
          continue;
        }
        return 0;
      }
      case EPIPE:
      case ECONNRESET: {
        if(++ers != 3) {
          continue;
        }
        ers = 0;
      }
      /* fallthrough */
      default: {
        info = info->ai_next;
        continue;
      }
    }
    assert(0);
    
    err:
    tcp_unlock(socket);
    return -1;
  }
  assert(0);
}

#define socket ((struct tcp_socket*) addr->data)

static void tcp_socket_connect_async(struct net_async_address* addr, struct addrinfo* info) {
  if(info == NULL) {
    tcp_socket_free_internal(socket);
  } else {
    if(tcp_socket_connect(socket, info) == -1) {
      tcp_socket_free_internal(socket);
    }
    net_free_address(info);
  }
  free(addr);
}

#undef socket

int tcp_socket(struct tcp_socket* const socket, const struct tcp_socket_options* const opt) {
  if(opt == NULL || (opt->info == NULL && opt->hostname == NULL && opt->port == NULL)) {
    errno = EINVAL;
    return -1;
  }
  {
    int err;
    safe_execute(err = pthread_mutex_init(&socket->lock, NULL), err != 0, err);
    if(err != 0) {
      errno = err;
      return -1;
    }
  }
  if(socket->loop == NULL) {
    socket->loop = shnet_calloc(1, sizeof(*socket->loop));
    if(socket->loop == NULL) {
      goto err_mutex;
    }
    if(tcp_async_loop(socket->loop) == -1) {
      free(socket->loop);
      socket->loop = NULL;
      goto err_mutex;
    }
    if(async_loop_start(socket->loop) == -1) {
      async_loop_free(socket->loop);
      free(socket->loop);
      socket->loop = NULL;
      goto err_mutex;
    }
    socket->alloc_loop = 1;
  }
  socket->core.fd = -1;
  socket->core.socket = 1;
  socket->core.server = 0;
  if(opt->info != NULL) {
    if(tcp_socket_connect(socket, opt->info) == -1) {
      goto err_loop;
    }
  } else {
    const size_t hostname_len = opt->hostname == NULL ? 0 : (strlen(opt->hostname) + 1);
    const size_t port_len = opt->port == NULL ? 0 : (strlen(opt->port) + 1);
    struct net_async_address* const async = shnet_malloc(sizeof(*async) + hostname_len + port_len + sizeof(struct addrinfo));
    if(async == NULL) {
      goto err_loop;
    }
    if(opt->hostname == NULL) {
      async->hostname = NULL;
      async->port = (char*)(async + 1);
      (void) memcpy(async->port, opt->port, port_len);
    } else {
      if(opt->port == NULL) {
        async->hostname = (char*)(async + 1);
        async->port = NULL;
        (void) memcpy(async->hostname, opt->hostname, hostname_len);
      } else {
        async->hostname = (char*)(async + 1);
        async->port = (char*)(async + 1) + hostname_len;
        (void) memcpy(async->hostname, opt->hostname, hostname_len);
        (void) memcpy(async->port, opt->port, port_len);
      }
    }
    async->data = socket;
    async->callback = tcp_socket_connect_async;
    struct addrinfo* const info = (struct addrinfo*)((char*)(async + 1) + hostname_len + port_len);
    info->ai_family = opt->family;
    info->ai_socktype = net_sock_stream;
    info->ai_protocol = net_proto_tcp;
    info->ai_flags = opt->flags;
    async->hints = info;
    if(net_get_address_async(async) == -1) {
      free(async);
      goto err_loop;
    }
  }
  return 0;
  
  err_loop:
  if(socket->alloc_loop) {
    async_loop_shutdown(socket->loop, async_free | async_ptr_free);
    socket->loop = NULL;
    socket->alloc_loop = 0;
  }
  err_mutex:
  (void) pthread_mutex_destroy(&socket->lock);
  return -1;
}

int tcp_send_buffered(struct tcp_socket* const socket) {
  while(!data_storage_is_empty(&socket->queue)) {
    ssize_t bytes;
    errno = 0;
#define data_ socket->queue.frames
    if(data_->file) {
      off_t off = data_->offset;
      safe_execute(bytes = sendfile(socket->core.fd, data_->fd, &off, data_->len - data_->offset), bytes == -1, errno);
    } else {
      safe_execute(bytes = send(socket->core.fd, data_->data + data_->offset, data_->len - data_->offset, MSG_NOSIGNAL), bytes == -1, errno);
    }
#undef data_
    if(bytes == -1) {
      switch(errno) {
        case EINTR: continue;
        case EPIPE:
        case ECONNRESET: {
          socket->closing_fast = 1;
          data_storage_free(&socket->queue);
          errno = EPIPE;
          return -2;
        }
        case EAGAIN: {
          data_storage_finish(&socket->queue);
          return -1;
        }
        default: return -1;
      }
    }
    data_storage_drain(&socket->queue, bytes);
  }
  if(!socket->close_guard && socket->closing) {
    (void) shutdown(socket->core.fd, SHUT_WR);
    errno = 0;
    return -2;
  }
  return 0;
}

int tcp_send(struct tcp_socket* const socket, const struct data_frame* const frame) {
  tcp_lock(socket);
  if(socket->closing || socket->closing_fast) {
    errno = EPIPE;
    goto err;
  }
  const int err = tcp_send_buffered(socket);
  if(err == -2) {
    errno = EPIPE;
    goto err;
  }
  if(!socket->dont_autoclean) {
    (void) data_storage_resize(&socket->queue, socket->queue.used);
  }
  if(err == -1 || !socket->opened) {
    errno = 0;
    if(data_storage_add(&socket->queue, frame) == -1) {
      goto err;
    }
    tcp_unlock(socket);
    return 0;
  }
  struct data_frame data = *frame;
  while(1) {
    ssize_t bytes;
    if(data.file) {
      off_t off = data.offset;
      safe_execute(bytes = sendfile(socket->core.fd, data.fd, &off, data.len - data.offset), bytes == -1, errno);
    } else {
      safe_execute(bytes = send(socket->core.fd, data.data + data.offset, data.len - data.offset, MSG_NOSIGNAL), bytes == -1, errno);
    }
    if(bytes == -1) {
      switch(errno) {
        case EINTR: continue;
        case EPIPE:
        case ECONNRESET: {
          socket->closing_fast = 1;
          goto err;
        }
        default: {
          const int ret = data_storage_add(&socket->queue, &data);
          tcp_unlock(socket);
          return ret;
        }
      }
      break;
    }
    data.offset += bytes;
    if(data.offset == data.len) {
      tcp_unlock(socket);
      data_storage_free_frame(frame);
      errno = 0;
      return 0;
    }
  }
  
  err:
  tcp_unlock(socket);
  data_storage_free_frame_err(frame);
  return -1;
}

uint64_t tcp_read(struct tcp_socket* const socket, void* data, uint64_t size) {
  if(size == 0) {
    errno = 0;
    return 0;
  }
  const uint64_t all = size;
  while(1) {
    ssize_t bytes;
    errno = 0;
    safe_execute(bytes = recv(socket->core.fd, data, size, 0), bytes == -1, errno);
    if(bytes == -1) {
      if(errno == EINTR) {
        continue;
      }
      break;
    } else if(bytes == 0) {
      errno = EPIPE;
      break;
    }
    size -= bytes;
    if(size == 0) {
      errno = 0;
      break;
    }
    data = (char*) data + bytes;
  }
  return all - size;
}

#define socket ((struct tcp_socket*) event)

static void tcp_socket_onevent(uint32_t events, struct async_event* event) {
  int code = 0;
  if(events & EPOLLERR) {
    (void) getsockopt(socket->core.fd, SOL_SOCKET, SO_ERROR, &code, &(socklen_t){ sizeof(int) });
  } else {
    if(!socket->opened && (events & EPOLLOUT)) {
      tcp_lock(socket);
      socket->opened = 1;
      tcp_unlock(socket);
      if(socket->on_event != NULL) {
        socket->on_event(socket, tcp_open);
      }
      tcp_lock(socket);
      if(!socket->close_guard) {
        if(socket->closing_fast) {
          data_storage_free(&socket->queue);
          socket->close_guard = 1;
          tcp_unlock(socket);
          (void) shutdown(socket->core.fd, SHUT_RDWR);
          events |= EPOLLHUP;
        } else if(socket->closing && data_storage_is_empty(&socket->queue)) {
          socket->close_guard = 1;
          tcp_unlock(socket);
          (void) shutdown(socket->core.fd, SHUT_WR);
        } else {
          tcp_unlock(socket);
        }
      } else {
        tcp_unlock(socket);
      }
      (void) getsockopt(socket->core.fd, SOL_SOCKET, SO_ERROR, &code, &(socklen_t){ sizeof(int) });
    }
    if((events & EPOLLIN) && socket->on_event != NULL) {
      socket->on_event(socket, tcp_data);
    }
  }
  if((events & EPOLLHUP) || code != 0) {
    errno = code;
    tcp_socket_free_internal(socket);
    return;
  }
  if(events & EPOLLOUT) {
    if(socket->on_event != NULL) {
      socket->on_event(socket, tcp_can_send);
    }
    if(!socket->dont_send_buffered) {
      tcp_lock(socket);
      if(!socket->closing_fast && tcp_send_buffered(socket) != -2 && !socket->dont_autoclean) {
        (void) data_storage_resize(&socket->queue, socket->queue.used);
      }
      tcp_unlock(socket);
    }
  }
  if(events & EPOLLRDHUP) {
    if(socket->on_event != NULL) {
      socket->on_event(socket, tcp_readclose);
    }
    if(!socket->dont_close_onreadclose) {
      tcp_socket_close(socket);
    }
  }
}

#undef socket



uint16_t tcp_server_get_port(const struct tcp_server* const server) {
  struct sockaddr_in6 addr;
  net_socket_get_local_address(server->core.fd, &addr);
  return net_address_to_port(&addr);
}

void tcp_server_free(struct tcp_server* const server) {
  (void) server->on_event(server, NULL, tcp_deinit);
  (void) close(server->core.fd);
  server->core.fd = -1;
  if(server->alloc_loop) {
    async_loop_shutdown(server->loop, async_free | async_ptr_free);
    server->loop = NULL;
    server->alloc_loop = 0;
  }
  (void) server->on_event(server, NULL, tcp_free);
}

void tcp_server_close(struct tcp_server* const server) {
  (void) shutdown(server->core.fd, SHUT_RDWR);
}

int tcp_server(struct tcp_server* const server, const struct tcp_server_options* const opt) {
  if(server->on_event == NULL || opt == NULL || (opt->info == NULL && opt->hostname == NULL && opt->port == NULL)) {
    errno = EINVAL;
    return -1;
  }
  struct addrinfo* info;
  struct addrinfo* cur_info;
  uint8_t alloc_info;
  if(opt->info == NULL) {
    const struct addrinfo hints = net_get_addr_struct(opt->family, net_sock_stream, net_proto_tcp, opt->flags | net_flag_wants_server);
    cur_info = net_get_address(opt->hostname, opt->port, &hints);
    if(cur_info == NULL) {
      return -1;
    }
    info = cur_info;
    alloc_info = 1;
  } else {
    info = NULL;
    cur_info = opt->info;
    alloc_info = 0;
  }
  if(server->loop == NULL) {
    server->loop = shnet_calloc(1, sizeof(*server->loop));
    if(server->loop == NULL) {
      goto err_addr;
    }
    if(tcp_async_loop(server->loop) == -1) {
      free(server->loop);
      server->loop = NULL;
      goto err_addr;
    }
    if(async_loop_start(server->loop) == -1) {
      async_loop_free(server->loop);
      free(server->loop);
      server->loop = NULL;
      goto err_addr;
    }
    server->alloc_loop = 1;
  }
  while(1) {
    server->core.fd = net_socket_get(cur_info);
    if(server->core.fd == -1) {
      goto err_loop;
    }
    net_socket_default_options(server->core.fd);
    if(net_socket_bind(server->core.fd, cur_info) == -1 || listen(server->core.fd, opt->backlog == 0 ? 32 : opt->backlog) == -1) {
      (void) close(server->core.fd);
      if(cur_info->ai_next == NULL) {
        goto err_loop;
      }
      cur_info = cur_info->ai_next;
    } else {
      break;
    }
  }
  server->core.socket = 0;
  server->core.server = 1;
  if(async_loop_add(server->loop, &server->core, EPOLLIN) == -1) {
    goto err_sfd;
  }
  if(alloc_info) {
    net_free_address(info);
  }
  return 0;
  
  err_sfd:
  (void) close(server->core.fd);
  server->core.fd = -1;
  err_loop:
  if(server->alloc_loop) {
    async_loop_shutdown(server->loop, async_free | async_ptr_free);
    server->loop = NULL;
    server->alloc_loop = 0;
  }
  err_addr:
  if(alloc_info) {
    net_free_address(info);
  }
  return -1;
}

#define _server ((struct tcp_server*) event)
#include <stdio.h>
static void tcp_server_onevent(uint32_t events, struct async_event* event) {
  if(events & EPOLLHUP) {
    (void) _server->on_event(_server, NULL, tcp_close);
    return;
  }
  assert(!(events & ~EPOLLIN));
  while(1) {
    struct sockaddr_storage addr;
    int sfd;
    safe_execute(sfd = accept(_server->core.fd, (struct sockaddr*)&addr, (socklen_t[]){ sizeof(addr) }), sfd == -1, errno);
    if(sfd == -1) {
      switch(errno) {
        case EPIPE: puts("EPIPE"); break;
        case ECONNRESET: puts("ECONNRESET"); break;
        case ECONNABORTED: puts("ECONNABORTED"); break;
        default: break;
      }
      switch(errno) {
        case EPIPE:
        case ECONNRESET:
        case ECONNABORTED: assert(0);
        default: break;
      }
      switch(errno) {
        case EINTR:
        case EPIPE:
        case EPERM:
        case EPROTO:
        case ECONNRESET:
        case ECONNABORTED: continue;
        default: return;
      }
    }
    struct tcp_socket sock = {0};
    sock.core.fd = sfd;
    sock.core.socket = 1;
    sock.core.server = 1;
    net_socket_default_options(sfd);
    struct tcp_socket* socket = _server->on_event(_server, &sock, tcp_open);
    if(socket == NULL) {
      goto err_sock;
    }
    if(socket == &sock) {
      void* const ptr = shnet_malloc(sizeof(*socket));
      if(ptr == NULL) {
        goto err_open;
      }
      socket = ptr;
      sock.free = 1;
    }
    *socket = sock;
    if(socket->loop == NULL) {
      socket->loop = _server->loop;
    }
    int err;
    safe_execute(err = pthread_mutex_init(&socket->lock, NULL), err != 0, err);
    if(err != 0) {
      errno = err;
      goto err_open;
    }
    if(async_loop_add(socket->loop, &socket->core, EPOLLET | EPOLLRDHUP | EPOLLIN | EPOLLOUT) == -1) {
      goto err_mutex;
    }
    continue;
    
    err_mutex:
    (void) pthread_mutex_destroy(&socket->lock);
    err_open:
    if(socket->free) {
      free(socket);
    }
    err_sock:
    (void) close(sfd);
  }
  assert(0);
}

#undef _server



void tcp_onevent(struct async_loop* loop, uint32_t events, struct async_event* event) {
  (void) loop;
  if(event->socket) {
    tcp_socket_onevent(events, event);
  } else {
    tcp_server_onevent(events, event);
  }
}

int tcp_async_loop(struct async_loop* const loop) {
  loop->on_event = tcp_onevent;
  return async_loop(loop);
}
