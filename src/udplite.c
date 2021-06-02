#include "udplite.h"

int udplite_create_socket(struct udp_socket* const sock) {
  if(udp_create_socket_base(sock, udp_lite_protocol) == net_failure) {
    return net_failure;
  }
  if(net_socket_base_options(sock->base.sfd) == net_failure || net_connect_socket(sock->base.sfd, &sock->addr) == net_failure) {
    udp_close(sock);
    return net_failure;
  }
  return net_success;
}

int udplite_create_server(struct udp_socket* const server) {
  if(udp_create_socket_base(server, udp_lite_protocol) == net_failure) {
    return net_failure;
  }
  if(net_socket_base_options(server->base.sfd) == net_failure || net_bind_socket(server->base.sfd, &server->addr) == net_failure) {
    udp_close(server);
    return net_failure;
  }
  return net_success;
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