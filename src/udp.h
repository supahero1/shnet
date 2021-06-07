#ifndef Akg_3FTcbgBTAIPX7pf___hmLu_IfH72
#define Akg_3FTcbgBTAIPX7pf___hmLu_IfH72 1

#include "net.h"

struct udp_socket {
  struct net_socket_base base;
};

extern int udp_create_socket_base(struct udp_socket* const, const int);

extern int udp_create_socket(struct udp_socket* const);

extern int udp_create_server(struct udp_socket* const);

extern void udp_close(struct udp_socket* const);

#endif // Akg_3FTcbgBTAIPX7pf___hmLu_IfH72