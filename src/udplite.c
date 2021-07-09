#include "udplite.h"

int udplite_create_socket(struct udp_socket* const sock) {
  if(udp_create_socket_base(sock, udp_lite_protocol) != 0) {
    return -1;
  }
  if(net_socket_base_options(sock->base.sfd) != 0 || net_connect_socket(sock->base.sfd, &sock->base.addr) != 0) {
    udp_close(sock);
    return -1;
  }
  return 0;
}

int udplite_create_server(struct udp_socket* const server) {
  if(udp_create_socket_base(server, udp_lite_protocol) != 0) {
    return -1;
  }
  if(net_socket_base_options(server->base.sfd) != 0 || net_bind_socket(server->base.sfd, &server->base.addr) != 0) {
    udp_close(server);
    return -1;
  }
  return 0;
}

int udplite_set_send_cscov(const struct udp_socket* const socket, const int coverage) {
  return setsockopt(socket->base.sfd, IPPROTO_UDPLITE, UDPLITE_SEND_CSCOV, &coverage, sizeof(int));
}

int udplite_get_send_cscov(const struct udp_socket* const socket, int* const coverage) {
  return getsockopt(socket->base.sfd, IPPROTO_UDPLITE, UDPLITE_SEND_CSCOV, coverage, &(socklen_t){sizeof(int)});
}

int udplite_set_min_allowed_cscov(const struct udp_socket* const socket, const int coverage) {
  return setsockopt(socket->base.sfd, IPPROTO_UDPLITE, UDPLITE_RECV_CSCOV, &coverage, sizeof(int));
}

int udplite_get_min_allowed_cscov(const struct udp_socket* const socket, int* const coverage) {
  return getsockopt(socket->base.sfd, IPPROTO_UDPLITE, UDPLITE_RECV_CSCOV, coverage, &(socklen_t){sizeof(int)});
}