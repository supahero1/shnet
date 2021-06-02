#ifndef Akg_3FTcbgBTAIPX7pf___hmLu_IfH72
#define Akg_3FTcbgBTAIPX7pf___hmLu_IfH72 1

#include "net.h"

struct udp_socket {
  struct net_socket_base base;
  struct sockaddr_in6 addr;
};

extern int udp_create_socket_base(struct udp_socket* const, const int);

extern int udp_create_socket(struct udp_socket* const);

extern int udp_create_server(struct udp_socket* const);

extern void udp_close(struct udp_socket* const);

extern void udp_close_sfd(const struct udp_socket* const);

extern int udp_socket_send(const struct udp_socket* const, const void* const, const unsigned short, const int);

extern int udp_server_send(const struct udp_socket* const, const struct sockaddr* const, const socklen_t, const void* const, const unsigned short, const int);

extern ssize_t udp_socket_read(const struct udp_socket* const, void* const, const ssize_t, const int);

extern ssize_t udp_server_read(const struct udp_socket* const, struct sockaddr* const, socklen_t* const, void* const, const ssize_t, const int);

#endif // Akg_3FTcbgBTAIPX7pf___hmLu_IfH72