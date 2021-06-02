#ifndef Iz0LHxFO4LQcKE9697kJvhSuLb6b26sA
#define Iz0LHxFO4LQcKE9697kJvhSuLb6b26sA 1

#include "udp.h"

#define IPPROTO_UDPLITE     136
#define UDPLITE_SEND_CSCOV  10
#define UDPLITE_RECV_CSCOV  11

extern int udplite_create_socket(struct udp_socket* const);

extern int udplite_create_server(struct udp_socket* const);

extern int udplite_set_send_cscov(const struct udp_socket* const, const int);

extern int udplite_get_send_cscov(const struct udp_socket* const, int* const);

extern int udplite_set_min_allowed_cscov(const struct udp_socket* const, const int);

extern int udplite_get_min_allowed_cscov(const struct udp_socket* const, int* const);

#endif // Iz0LHxFO4LQcKE9697kJvhSuLb6b26sA