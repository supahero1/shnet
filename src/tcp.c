#include <errno.h>
#include <fcntl.h>
#include <assert.h>
#include <unistd.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <stdatomic.h>
#include <sys/ioctl.h>
#include <netinet/tcp.h>
#include <sys/sendfile.h>

#include <shnet/tcp.h>
#include <shnet/error.h>

#define tcp_add_flag(a,b) atomic_fetch_or_explicit(&(a)->core.flags, b, memory_order_acq_rel)
#define tcp_test_flag(a,b) (atomic_load_explicit(&(a)->core.flags, memory_order_acquire) & (b))
#define tcp_remove_flag(a,b) atomic_fetch_and_explicit(&(a)->core.flags, ~(b), memory_order_acq_rel)
#define tcp_clear_flag(a) atomic_store_explicit(&(a)->core.flags, 0, memory_order_release)

#define tcp_lock(a) (void) pthread_mutex_lock(&(a)->lock)
#define tcp_unlock(a) (void) pthread_mutex_unlock(&(a)->lock)



void tcp_socket_cork_on(struct tcp_socket* const socket) {
  (void) net_socket_setopt_true(socket->core.fd, net_proto_tcp, TCP_CORK);
}

void tcp_socket_cork_off(struct tcp_socket* const socket) {
  (void) net_socket_setopt_false(socket->core.fd, net_proto_tcp, TCP_CORK);
}

void tcp_socket_nodelay_on(struct tcp_socket* const socket) {
  (void) net_socket_setopt_true(socket->core.fd, net_proto_tcp, TCP_NODELAY);
}

void tcp_socket_nodelay_off(struct tcp_socket* const socket) {
  (void) net_socket_setopt_false(socket->core.fd, net_proto_tcp, TCP_NODELAY);
}

void tcp_socket_keepalive_explicit(const struct tcp_socket* const socket, const int idle_time, const int reprobe_time, const int retries) {
  (void) setsockopt(socket->core.fd, net_proto_tcp, TCP_KEEPCNT, &retries, sizeof(int));
  (void) setsockopt(socket->core.fd, net_proto_tcp, TCP_KEEPIDLE, &idle_time, sizeof(int));
  (void) setsockopt(socket->core.fd, net_proto_tcp, TCP_KEEPINTVL, &reprobe_time, sizeof(int));
  (void) net_socket_setopt_true(socket->core.fd, SOL_SOCKET, SO_KEEPALIVE);
  (void) setsockopt(socket->core.fd, net_proto_tcp, TCP_USER_TIMEOUT, (int[]){ (idle_time + reprobe_time * retries) * 1000 }, sizeof(int));
}

void tcp_socket_keepalive(const struct tcp_socket* const socket) {
  tcp_socket_keepalive_explicit(socket, 1, 1, 10);
}

void tcp_socket_dont_receive_data(struct tcp_socket* const socket) {
  (void) shutdown(socket->core.fd, SHUT_RD);
}

int tcp_socket_unread_data(const struct tcp_socket* const socket) {
  int val = 0;
  (void) ioctl(socket->core.fd, FIONREAD, &val);
  return val;
}

int tcp_socket_unsent_data(const struct tcp_socket* const socket) {
  int val = 0;
  (void) ioctl(socket->core.fd, TIOCOUTQ, &val);
  return val;
}

static void tcp_socket_free_(struct tcp_socket* const socket) {
  if(socket->core.fd != -1) {
    (void) close(socket->core.fd);
  }
  (void) pthread_mutex_destroy(&socket->lock);
  data_storage_free(&socket->queue);
  if(socket->alloc_loop) {
    async_loop_push_free(socket->loop);
    async_loop_push_ptr_free(socket->loop);
    async_loop_commit(socket->loop);
    socket->loop = NULL;
    socket->alloc_loop = 0;
  }
  uint8_t free_ = socket->free;
  socket->core.fd = -1;
  /*
   * This meaningless (at this point) bit can be used to
   * notify the on_event() handler about which socket this
   * really is (client or a server socket, don't confuse
   * with listening socket). This can be done, because the
   * bit doesn't change behavior of tcp_socket().
   */
  socket->core.socket = socket->core.server ^ 1;
  socket->opened = 0;
  socket->free = 0;
  socket->close_guard = 0;
  if(socket->on_event != NULL) {
    socket->on_event(socket, tcp_free);
  }
  if(free_) {
    free(socket);
  }
}

void tcp_socket_free(struct tcp_socket* const socket) {
  if(tcp_add_flag(socket, tcp_confirmed_free) & tcp_confirmed_free) {
    tcp_socket_free_(socket);
  }
}

static void tcp_socket_free_internal(struct tcp_socket* const socket) {
  if(socket->on_event != NULL) {
    socket->on_event(socket, tcp_close);
  }
  if(tcp_add_flag(socket, tcp_confirmed_free) & tcp_confirmed_free) {
    tcp_socket_free_(socket);
  } else {
    (void) async_loop_remove(socket->loop, &socket->core);
  }
}

void tcp_socket_close(struct tcp_socket* const socket) {
  tcp_add_flag(socket, tcp_closing | tcp_shutdown_wr);
  tcp_lock(socket);
  if(!socket->close_guard) {
    if(socket->core.fd != -1 && data_storage_is_empty(&socket->queue)) {
      (void) shutdown(socket->core.fd, SHUT_WR);
      socket->close_guard = 1;
    }
  }
  tcp_unlock(socket);
}

void tcp_socket_force_close(struct tcp_socket* const socket) {
  tcp_add_flag(socket, tcp_closing_fast | tcp_shutdown_wr);
  tcp_lock(socket);
  if(!socket->close_guard) {
    data_storage_free(&socket->queue);
    if(socket->core.fd != -1) {
      (void) shutdown(socket->core.fd, SHUT_RDWR);
      socket->close_guard = 1;
    }
  }
  tcp_unlock(socket);
}

static int tcp_socket_connect(struct tcp_socket* const socket, const struct addrinfo* const info) {
  const struct addrinfo* cur_info = info;
  while(1) {
    tcp_lock(socket);
    if(socket->core.fd != -1) {
      (void) close(socket->core.fd);
      socket->core.fd = -1;
    }
    if(cur_info == NULL) {
      tcp_unlock(socket);
      return -1;
    }
    socket->core.fd = net_socket_get(cur_info);
    if(socket->core.fd == -1) {
      tcp_unlock(socket);
      return -1;
    }
    net_socket_default_options(socket->core.fd);
    tcp_unlock(socket);
    errno = 0;
    (void) net_socket_connect(socket->core.fd, cur_info);
    switch(errno) {
      case 0:
      case EINTR:
      case EINPROGRESS: {
        if(async_loop_add(socket->loop, &socket->core, EPOLLET | EPOLLRDHUP | EPOLLIN | EPOLLOUT) == -1) {
          cur_info = NULL;
          continue;
        }
        return 0;
      }
      case EPIPE:
      case ECONNRESET: continue;
      default: {
        cur_info = cur_info->ai_next;
        continue;
      }
    }
  }
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
  socket->core.fd = -1;
  socket->core.socket = 1;
  {
    int err;
    safe_execute(err = pthread_mutex_init(&socket->lock, NULL), err != 0, err);
    if(err != 0) {
      errno = err;
      return -1;
    }
  }
  if(socket->loop == NULL) {
    safe_execute(socket->loop = calloc(1, sizeof(*socket->loop)), socket->loop == NULL, ENOMEM);
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
  tcp_clear_flag(socket);
  if(opt->info != NULL) {
    if(tcp_socket_connect(socket, opt->info) == -1) {
      goto err_loop;
    }
  } else {
    const size_t hostname_len = opt->hostname == NULL ? 0 : (strlen(opt->hostname) + 1);
    const size_t port_len = opt->port == NULL ? 0 : (strlen(opt->port) + 1);
    struct net_async_address* async;
    safe_execute(async = malloc(sizeof(*async) + hostname_len + port_len + sizeof(struct addrinfo)), async == NULL, ENOMEM);
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
    async_loop_push_free(socket->loop);
    async_loop_push_ptr_free(socket->loop);
    async_loop_commit(socket->loop);
    socket->loop = NULL;
    socket->alloc_loop = 0;
  }
  err_mutex:
  (void) pthread_mutex_destroy(&socket->lock);
  return -1;
}

static int tcp_send_buffered(struct tcp_socket* const socket) {
  if(!data_storage_is_empty(&socket->queue)) {
    while(1) {
      ssize_t bytes;
#define data_ socket->queue.frames
      if(socket->queue.frames->file) {
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
            tcp_add_flag(socket, tcp_shutdown_wr);
            data_storage_free(&socket->queue);
            return -2;
          }
          case EAGAIN: {
            tcp_remove_flag(socket, tcp_send_available);
            data_storage_finish(&socket->queue);
            if(!socket->dont_autoclean) {
              (void) data_storage_resize(&socket->queue, socket->queue.used);
            }
            return -1;
          }
          default: return -1;
        }
      }
      if(data_storage_drain(&socket->queue, bytes)) {
        break;
      }
    }
  }
  if(tcp_test_flag(socket, tcp_closing)) {
    (void) shutdown(socket->core.fd, SHUT_WR);
    tcp_add_flag(socket, tcp_shutdown_wr);
    return -2;
  }
  return 0;
}

int tcp_send(struct tcp_socket* const socket, const struct data_frame* const frame) {
  if(tcp_test_flag(socket, tcp_shutdown_wr)) {
    errno = EPIPE;
    goto err;
  }
  tcp_lock(socket);
  if(!tcp_test_flag(socket, tcp_send_available)) {
    errno = 0;
    const int ret = data_storage_add(&socket->queue, frame);
    tcp_unlock(socket);
    return ret;
  }
  const int err = tcp_send_buffered(socket);
  if(err == -2) {
    tcp_unlock(socket);
    goto err;
  }
  if(err == -1) {
    errno = 0;
    const int ret = data_storage_add(&socket->queue, frame);
    tcp_unlock(socket);
    return ret;
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
          tcp_add_flag(socket, tcp_shutdown_wr);
          tcp_unlock(socket);
          goto err;
        }
        case EAGAIN: {
          tcp_remove_flag(socket, tcp_send_available);
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
      errno = 0;
      tcp_unlock(socket);
      data_storage_free_frame(&data);
      return 0;
    }
  }
  
  err:
  data_storage_free_frame_err(&data);
  return -1;
}

uint64_t tcp_read(struct tcp_socket* const socket, void* data, uint64_t size) {
  const uint64_t all = size;
  while(1) {
    ssize_t bytes;
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
    (void) getsockopt(socket->core.fd, SOL_SOCKET, SO_ERROR, &code, &(socklen_t){sizeof(int)});
  } else if(!(events & EPOLLHUP)) {
    if(!socket->opened && (events & EPOLLOUT)) {
      socket->opened = 1;
      if(socket->on_event != NULL) {
        socket->on_event(socket, tcp_open);
      }
      tcp_lock(socket);
      if(!socket->close_guard) {
        if(tcp_test_flag(socket, tcp_closing_fast)) {
          (void) shutdown(socket->core.fd, SHUT_RDWR);
          data_storage_free(&socket->queue);
        } else if(data_storage_is_empty(&socket->queue) && tcp_test_flag(socket, tcp_closing)) {
          (void) shutdown(socket->core.fd, SHUT_WR);
        }
      }
      tcp_unlock(socket);
      (void) getsockopt(socket->core.fd, SOL_SOCKET, SO_ERROR, &code, &(socklen_t){sizeof(int)});
    }
    if((events & EPOLLIN) && socket->on_event != NULL) {
      socket->on_event(socket, tcp_data);
    }
  }
  if((events & EPOLLHUP) || code != 0) {
    if(code != 0) {
      errno = code;
    }
    tcp_socket_free_internal(socket);
    return;
  }
  if(events & EPOLLOUT) {
    tcp_add_flag(socket, tcp_send_available);
    if(socket->on_event != NULL) {
      socket->on_event(socket, tcp_can_send);
    }
    if(!socket->dont_send_buffered) {
      tcp_lock(socket);
      if(tcp_test_flag(socket, tcp_send_available) && tcp_send_buffered(socket) == -2) {
        tcp_socket_free_internal(socket);
        return;
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
  (void) close(server->core.fd);
  server->core.fd = -1;
  if(server->alloc_loop) {
    async_loop_push_free(server->loop);
    async_loop_push_ptr_free(server->loop);
    async_loop_commit(server->loop);
    server->loop = NULL;
    server->alloc_loop = 0;
  }
  server->alloc_info = 0;
  (void) server->on_event(server, tcp_free, NULL);
}

int tcp_server_close(struct tcp_server* const server) {
  tcp_add_flag(server, tcp_closing);
  return async_loop_create_event(server->loop, &server->core);
}

int tcp_server(struct tcp_server* const server, const struct tcp_server_options* const opt) {
  if(server->on_event == NULL || opt == NULL || (opt->info == NULL && opt->hostname == NULL && opt->port == NULL) || server->alloc_info != 0/* Double initialisation */) {
    errno = EINVAL;
    return -1;
  }
  struct addrinfo* info;
  struct addrinfo* cur_info;
  if(opt->info == NULL) {
    const struct addrinfo hints = net_get_addr_struct(opt->family, net_sock_stream, net_proto_tcp, opt->flags);
    cur_info = net_get_address(opt->hostname, opt->port, &hints);
    if(cur_info == NULL) {
      return -1;
    }
    info = cur_info;
    server->alloc_info = 1;
  } else {
    info = NULL;
    cur_info = opt->info;
  }
  if(server->loop == NULL) {
    safe_execute(server->loop = calloc(1, sizeof(*server->loop)), server->loop == NULL, ENOMEM);
    if(server->loop == NULL) {
      goto err_addr;
    }
    if(tcp_async_loop(server->loop) == -1) {
      free(server->loop);
      goto err_addr;
    }
    if(async_loop_start(server->loop) == -1) {
      async_loop_free(server->loop);
      free(server->loop);
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
        server->core.fd = -1;
        goto err_loop;
      }
      cur_info = cur_info->ai_next;
    } else {
      break;
    }
  }
  tcp_clear_flag(server);
  if(async_loop_add(server->loop, &server->core, EPOLLET | EPOLLIN) == -1) {
    goto err_sfd;
  }
  if(server->alloc_info) {
    net_free_address(info);
  }
  return 0;
  
  err_sfd:
  (void) close(server->core.fd);
  server->core.fd = -1;
  err_loop:
  if(server->alloc_loop) {
    async_loop_push_free(server->loop);
    async_loop_push_ptr_free(server->loop);
    async_loop_commit(server->loop);
    server->loop = NULL;
    server->alloc_loop = 0;
  }
  err_addr:
  if(server->alloc_info) {
    net_free_address(info);
    server->alloc_info = 0;
  }
  return -1;
}

#define _server ((struct tcp_server*) event)

void tcp_server_onevent(uint32_t events, struct async_event* event) {
  if(!events) {
    (void) _server->on_event(_server, tcp_close, NULL);
    return;
  }
  assert(!(events & ~EPOLLIN));
  while(1) {
    if(tcp_test_flag(_server, tcp_closing)) {
      return;
    }
    struct sockaddr_in6 addr;
    int sfd;
    safe_execute(sfd = accept(_server->core.fd, (struct sockaddr*)&addr, (socklen_t[]){ sizeof(addr) }), sfd == -1, errno);
    if(sfd == -1) {
      if(errno == EAGAIN) {
        return;
      }
      continue;
    }
    struct tcp_socket socket = {0};
    socket.core.fd = sfd;
    struct tcp_socket* sock = _server->on_event(_server, tcp_open, &socket);
    if(!sock) {
      goto err_sock;
    }
    socket.core.socket = 1;
    socket.core.server = 1;
    if(socket.loop == NULL) {
      socket.loop = _server->loop;
    }
    if(sock == &socket) {
      safe_execute(sock = malloc(sizeof(*sock)), sock == NULL, ENOMEM);
      if(sock == NULL) {
        goto err_sock;
      }
      socket.free = 1;
    }
    *sock = socket;
    net_socket_default_options(sock->core.fd);
    int err;
    safe_execute(err = pthread_mutex_init(&sock->lock, NULL), err != 0, err);
    if(err != 0) {
      goto err_sock;
    }
    tcp_clear_flag(sock);
    if(async_loop_add(sock->loop, &sock->core, EPOLLET | EPOLLRDHUP | EPOLLIN | EPOLLOUT) == -1) {
      goto err_mutex;
    }
    continue;
    
    err_mutex:
    (void) pthread_mutex_destroy(&socket.lock);
    err_sock:
    (void) close(sfd);
  }
  assert(0);
}

#undef _server



static void tcp_onevent(struct async_loop* loop, uint32_t events, struct async_event* event) {
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



#undef tcp_unlock
#undef tcp_lock

#undef tcp_clear_flag
#undef tcp_remove_flag
#undef tcp_test_flag
#undef tcp_add_flag