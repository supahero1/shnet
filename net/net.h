/*
   Copyright 2021 shädam

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at

       http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License.
*/

#ifndef W_pOOj__2VkXM_ew9zpRf2pZH_H8ggTe
#define W_pOOj__2VkXM_ew9zpRf2pZH_H8ggTe 1

#ifdef __cplusplus
extern "C" {
#endif

#include "../def.h"
#include "net_base.h"
#include "../net/net_avl.h"
#include "../timeout/timeout.h"

#include <semaphore.h>

#define IPv4       AF_INET
#define IPv6       AF_INET6
#define IP_ANY     AF_UNSPEC
#define NET_SERVER AI_PASSIVE

extern void TCPNoBlock(const int);

extern void TCPCorkOn(const int);

extern void TCPCorkOff(const int);

extern void TCPNoDelayOn(const int);

extern void TCPNoDelayOff(const int);

extern uint16_t TCPGetHandshakeTimeout(struct NETServer* const);

extern void TCPSetHandshakeTimeout(struct NETServer* const, const uint16_t);

__nonnull((1))
void TCPSocketFree(struct NETSocket* const);

__nonnull((1))
void TCPServerFree(struct NETServer* const);

__nonnull((2))
extern int TCPSend(struct NETSocket* const, void* const, const size_t, struct NETConnManager* const);

__nonnull((1))
extern void TCPSendShutdown(struct NETSocket* const);

__nonnull((1))
extern void TCPReceiveShutdown(struct NETSocket* const);

__nonnull((1))
extern void TCPShutdown(struct NETSocket* const);

__nonnull((4))
extern int GetAddrInfo(const char* const, const char* const, const int, struct addrinfo** const);

struct ANET_GAILookup {
  void (*handler)(struct addrinfo*, int);
  char* hostname;
  char* service;
  int flags;
};

struct ANET_GAIArray {
  struct ANET_GAILookup* arr;
  uint_fast32_t count;
};

__nonnull((1))
extern int AsyncGetAddrInfo(struct ANET_GAIArray* const);

__nonnull((1))
extern int SyncTCPConnect(struct addrinfo* const, struct NETSocket* const);

__nonnull((1))
extern int SyncTCP_GAIConnect(const char* const, const char* const, const int, struct NETSocket* const);

__nonnull((1))
extern int SyncTCP_IP_GAIConnect(const char* const, const char* const, const int, struct NETSocket* const);

__nonnull((1))
extern int SyncTCPListen(struct addrinfo* const, struct NETServer* const);

__nonnull((1))
extern int SyncTCP_GAIListen(const char* const, const char* const, const int, struct NETServer* const);

struct NETConnManager {
  pthread_t thread;
  struct net_avl_tree avl_tree;
  int epoll;
  pthread_mutex_t mutex;
};

__nonnull((1))
extern int InitConnManager(struct NETConnManager* const, const uint32_t);

__nonnull((1))
extern int InitEventlessConnManager(struct NETConnManager* const, const uint32_t);

__nonnull((1))
extern int AddSocket(struct NETConnManager* const, const struct NETSocket);

__nonnull((1))
extern int AddServer(struct NETConnManager* const, const struct NETServer);

__nonnull((1))
extern struct NETSocket* GetSocket(struct NETConnManager* const, const int);

__nonnull((1))
extern struct NETServer* GetServer(struct NETConnManager* const, const int);

__nonnull((1))
extern int DeleteSocket(struct NETConnManager* const, const int);

__nonnull((1))
extern int DeleteServer(struct NETConnManager* const, const int);

__nonnull((1))
extern void DeleteEventlessSocket(struct NETConnManager* const, const int);

__nonnull((1))
extern void DeleteEventlessServer(struct NETConnManager* const, const int);

__nonnull((1))
extern void FreeConnectionManager(struct NETConnManager* const);

struct NETServerThreadPool {
  pthread_t* threads;
  void (*onclose)(struct NETServerThreadPool*);
  struct NETServer* server;
  sem_t semaphore;
  _Atomic uint32_t amount;
  _Atomic uint32_t state;
  uint32_t growth;
};

__nonnull((1))
extern int InitServerThreadPool(struct NETServerThreadPool* const, const uint32_t, const uint32_t, const int);

__nonnull((1))
extern void FreeServerThreadPool(struct NETServerThreadPool* const);

struct ANET_C {
  void (*handler)(struct NETSocket*, int);
  struct addrinfo* addrinfo;
};

__nonnull((1))
extern int AsyncTCPConnect(struct ANET_C* const);

struct ANET_L {
  void (*handler)(struct NETServer*, int);
  struct addrinfo* addrinfo;
};

__nonnull((1))
extern int AsyncTCPListen(struct ANET_L* const);

#ifdef __cplusplus
}
#endif

#endif // W_pOOj__2VkXM_ew9zpRf2pZH_H8ggTe