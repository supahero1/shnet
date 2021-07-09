#include "udp.h"

#include <unistd.h>

int udp_create_socket_base(struct udp_socket* const sock, const int proto) {
  const int sfd = socket(net_get_family(&sock->base.addr), datagram_socktype, proto);
  if(sfd == -1) {
    return -1;
  }
  sock->base.sfd = sfd;
  return 0;
}

int udp_create_socket(struct udp_socket* const socket) {
  if(udp_create_socket_base(socket, udp_protocol) != 0) {
    return -1;
  }
  if(net_socket_base_options(socket->base.sfd) != 0 || net_connect_socket(socket->base.sfd, &socket->base.addr) != 0) {
    udp_close(socket);
    return -1;
  }
  socket->base.which = net_socket;
  return 0;
}

int udp_create_server(struct udp_socket* const server) {
  if(udp_create_socket_base(server, udp_protocol) != 0) {
    return -1;
  }
  if(net_socket_base_options(server->base.sfd) != 0 || net_bind_socket(server->base.sfd, &server->base.addr) != 0) {
    udp_close(server);
    return -1;
  }
  server->base.which = net_server;
  return 0;
}

void udp_close(struct udp_socket* const socket) {
  (void) close(socket->base.sfd);
}