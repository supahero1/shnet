#ifndef W_pOOj__2VkXM_ew9zpRf2pZH_H8ggTe
#define W_pOOj__2VkXM_ew9zpRf2pZH_H8ggTe 1

#ifdef __cplusplus
extern "C" {
#endif

#include <netdb.h>
#include <stdint.h>
#include <signal.h>

#include "../def.h"
#include "../net/net_avl.h"
#include "../distr/distr.h"

/*struct HTTP_URL {
  char* protocol;
  char* host;
  char* origin;
  char* port;
  char* pathname;
  char* query;
  char* hash;
  
  uint16_t origin_len;
  uint16_t pathname_len;
  uint16_t query_len;
  uint16_t hash_len;
  uint8_t protocol_len;
  uint8_t host_len;
  uint8_t port_len;
};

__nothrow __nonnull((1))
extern struct HTTP_URL ParseHTTP_URL(const char* const);

struct HTTPSocket {
  uint8_t state;
};*/

enum NETFlags { NET_CONNECTING, NET_OPEN, NET_CLOSED,
  NET_GAI   = 1,
  NET_NOGAI = 2
};

struct NETSocket {
  struct sockaddr addr;
  char* canonname;
  socklen_t addrlen;
  enum NETFlags state;
  int flags;
  int family;
  int socktype;
  int protocol;
  int sfd;
};

struct AsyncGAIList {
  void (*callback)(struct addrinfo*, int);
  char* hostname;
  char* service;
  int flags;
};

struct AsyncGAI {
  void (*callback)(struct NETSocket*, int);
  char* hostname;
  char* service;
  int flags;
};

struct AsyncNoGAI {
  void (*callback)(struct NETSocket*, int);
  struct addrinfo* addrinfos;
};

union GAIInfo {
  struct AsyncGAI* gai;
  struct AsyncNoGAI* nogai;
};

struct AsyncNETArray {
  union GAIInfo infos;
  uint32_t count;
  enum NETFlags type;
};

struct AsyncGAIArray {
  struct AsyncGAIList* infos;
  uint32_t count;
};

__nothrow __nonnull((4))
extern int GetAddrInfo(const char* const, const char* const, const int, struct addrinfo** const restrict);

__nothrow __nonnull((1))
extern int AsyncGetAddrInfo(struct AsyncGAIArray* const);

__nothrow __nonnull((1))
extern int AsyncTCPConnect(struct AsyncNETArray* const);

__nothrow __nonnull((1))
extern int AsyncTCPListen(struct AsyncNETArray* const);

struct NETConnectionManager {
  pthread_t thread;
  struct net_avl_tree avl_tree; // store socket info too
  int epoll;
};

__nothrow __nonnull((1))
extern int InitConnectionManager(struct NETConnectionManager* const, const uint32_t);

__nothrow __nonnull((1))
extern int AddSocket(struct NETConnectionManager* const, const int, void (*)(int, uint32_t));

__nothrow __nonnull((1))
extern int DeleteSocket(struct NETConnectionManager* const, const int);

__nothrow __nonnull((1))
extern void FreeConnectionManager(struct NETConnectionManager* const);

#ifdef __cplusplus
}
#endif

#endif // W_pOOj__2VkXM_ew9zpRf2pZH_H8ggTe
