#ifndef Iz0LHxFO4LQcKE9697kJvhSuLb6b26sA
#define Iz0LHxFO4LQcKE9697kJvhSuLb6b26sA 1

#include "udp.h"

#ifndef IPPROTO_UDPLITE
#define IPPROTO_UDPLITE     136
#endif
#ifndef UDPLITE_SEND_CSCOV
#define UDPLITE_SEND_CSCOV  10
#endif
#ifndef UDPLITE_RECV_CSCOV
#define UDPLITE_RECV_CSCOV  11
#endif

extern int udplite_create_socket(struct udp_socket* const);

extern int udplite_create_server(struct udp_socket* const);

extern int udplite_set_send_cscov(const struct udp_socket* const, const int);

extern int udplite_get_send_cscov(const struct udp_socket* const, int* const);

extern int udplite_set_min_allowed_cscov(const struct udp_socket* const, const int);

extern int udplite_get_min_allowed_cscov(const struct udp_socket* const, int* const);

#endif // Iz0LHxFO4LQcKE9697kJvhSuLb6b26sA