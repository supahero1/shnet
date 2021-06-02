#include "udp.h"

#include <errno.h>
#include <unistd.h>

int udp_create_socket_base(struct udp_socket* const sock, const int proto) {
  const int sfd = socket(net_get_family(&sock->addr), datagram_socktype, proto);
  if(sfd == -1) {
    return net_failure;
  }
  sock->base.sfd = sfd;
  return net_success;
}

int udp_create_socket(struct udp_socket* const sock) {
  if(udp_create_socket_base(sock, udp_protocol) == net_failure) {
    return net_failure;
  }
  if(net_socket_base_options(sock->base.sfd) == net_failure || net_connect_socket(sock->base.sfd, &sock->addr) == net_failure) {
    udp_close(sock);
    return net_failure;
  }
  return net_success;
}

int udp_create_server(struct udp_socket* const server) {
  if(udp_create_socket_base(server, udp_protocol) == net_failure) {
    return net_failure;
  }
  if(net_socket_base_options(server->base.sfd) == net_failure || net_bind_socket(server->base.sfd, &server->addr) == net_failure) {
    udp_close(server);
    return net_failure;
  }
  return net_success;
}

void udp_close(struct udp_socket* const socket) {
  (void) close(socket->base.sfd);
}

static int udp_send_internal(const struct udp_socket* const socket, const struct sockaddr* const sockaddr, const socklen_t socklen, const void* const data, const unsigned short size, const int flags) {
  beginning:
  if(sendto(socket->base.sfd, data, size, flags, sockaddr, socklen) == -1) {
    if(errno == EINTR) {
      goto beginning;
    }
    return net_failure;
  }
  return net_success;
}

int udp_socket_send(const struct udp_socket* const socket, const void* const data, const unsigned short size, const int flags) {
  return udp_send_internal(socket, NULL, 0, data, size, flags);
}

int udp_server_send(const struct udp_socket* const server, const struct sockaddr* const sockaddr, const socklen_t socklen, const void* const data, const unsigned short size, const int flags) {
  return udp_send_internal(server, sockaddr, socklen, data, size, flags);
}

static ssize_t udp_read_internal(const struct udp_socket* const socket, struct sockaddr* const sockaddr, socklen_t* const socklen, void* const data, const ssize_t size, const int flags) {
  ssize_t bytes;
  do {
    bytes = recvfrom(socket->base.sfd, data, size, flags, sockaddr, socklen);
  } while(bytes == -1 && errno == EINTR);
  return bytes;
}

ssize_t udp_socket_read(const struct udp_socket* const socket, void* const data, const ssize_t size, const int flags) {
  return udp_read_internal(socket, NULL, NULL, data, size, flags);
}

ssize_t udp_server_read(const struct udp_socket* const server, struct sockaddr* const sockaddr, socklen_t* const socklen, void* const data, const ssize_t size, const int flags) {
  return udp_read_internal(server, sockaddr, socklen, data, size, flags);
}