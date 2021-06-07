#include "udp.h"

#include <unistd.h>

int udp_create_socket_base(struct udp_socket* const sock, const int proto) {
  const int sfd = socket(net_get_family(&sock->base.addr), datagram_socktype, proto);
  if(sfd == -1) {
    return net_failure;
  }
  sock->base.sfd = sfd;
  return net_success;
}

int udp_create_socket(struct udp_socket* const socket) {
  if(udp_create_socket_base(socket, udp_protocol) == net_failure) {
    return net_failure;
  }
  if(net_socket_base_options(socket->base.sfd) == net_failure || net_connect_socket(socket->base.sfd, &socket->base.addr) != 0) {
    udp_close(socket);
    return net_failure;
  }
  return net_success;
}

int udp_create_server(struct udp_socket* const server) {
  if(udp_create_socket_base(server, udp_protocol) == net_failure) {
    return net_failure;
  }
  if(net_socket_base_options(server->base.sfd) == net_failure || net_bind_socket(server->base.sfd, &server->base.addr) != 0) {
    udp_close(server);
    return net_failure;
  }
  return net_success;
}

void udp_close(struct udp_socket* const socket) {
  (void) close(socket->base.sfd);
}