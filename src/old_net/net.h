/*
  Copyright (c) 2021 shädam

  Permission is hereby granted, free of charge, to any person obtaining a copy
  of this software and associated documentation files (the "Software"), to deal
  in the Software without restriction, including without limitation the rights
  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
  copies of the Software, and to permit persons to whom the Software is
  furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included in all
  copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
  SOFTWARE.
*/

#ifndef W_pOOj__2VkXM_ew9zpRf2pZH_H8ggTe
#define W_pOOj__2VkXM_ew9zpRf2pZH_H8ggTe 1

#ifdef __cplusplus
extern "C" {
#endif

#include "net_avl.h"
#include "../time/time.h"

enum NETCodes {
  NET_IPv4 = AF_INET,
  NET_IPv6 = AF_INET6,
  NET_IP_ANY = AF_UNSPEC,
  NET_SERVER = AI_PASSIVE,
  NET_TLS = 0x8000000
};

extern void TCPNoBlock(const int);

extern void TCPCorkOn(const int);

extern void TCPCorkOff(const int);

extern void TCPNoDelayOn(const int);

extern void TCPNoDelayOff(const int);

extern void TCPIdleTimeout(const int, const int);

extern void TCPSocketFree(struct NETSocket* const);

extern void TCPServerFree(struct NETServer* const);

struct NETConnManager {
  void (*onstart)(struct NETConnManager*);
  pthread_t thread;
  #if __WORDSIZE == 64
  struct net_avl_multithread_tree tree;
  int epoll;
  #else
  int epoll;
  struct net_avl_multithread_tree tree;
  #endif
};

extern int TCPSend(struct NETSocket* const, void* const, const ssize_t, struct NETConnManager* const);

extern void TCPSendShutdown(struct NETSocket* const);

extern void TCPReceiveShutdown(struct NETSocket* const);

extern void TCPShutdown(struct NETSocket* const);

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

extern int AsyncGetAddrInfo(struct ANET_GAIArray* const);

extern int SyncTCPConnect(struct addrinfo* const, struct NETSocket* const);

extern int SyncTCP_GAIConnect(const char* const, const char* const, const int, struct NETSocket* const);

extern int SyncTCP_IP_GAIConnect(const char* const, const char* const, const int, struct NETSocket* const);

extern int SyncTCPListen(struct addrinfo* const, struct NETServer* const);

extern int SyncTCP_GAIListen(const char* const, const char* const, const int, struct NETServer* const);

struct ANET_C {
  void (*handler)(struct NETSocket*, int);
  struct addrinfo* addrinfo;
};

extern int AsyncTCPConnect(struct ANET_C* const);

struct ANET_L {
  void (*handler)(struct NETServer*, int);
  struct addrinfo* addrinfo;
};

extern int AsyncTCPListen(struct ANET_L* const);

extern int InitConnManager(struct NETConnManager* const, const uint32_t);

extern int InitEventlessConnManager(struct NETConnManager* const, const uint32_t);

extern int AddSocket(struct NETConnManager* const, const struct NETSocket);

extern int AddServer(struct NETConnManager* const, const struct NETServer);

extern struct NETSocket* GetSocket(struct NETConnManager* const, const int);

extern struct NETServer* GetServer(struct NETConnManager* const, const int);

extern int DeleteSocket(struct NETConnManager* const, const int);

extern int DeleteServer(struct NETConnManager* const, const int);

extern void DeleteEventlessSocket(struct NETConnManager* const, const int);

extern void DeleteEventlessServer(struct NETConnManager* const, const int);

extern void FreeConnectionManager(struct NETConnManager* const);

struct NETAcceptThreadPool {
  pthread_t* threads;
  void (*onstart)(struct NETAcceptThreadPool*);
  void (*onstop)(struct NETAcceptThreadPool*);
  void (*onerror)(struct NETAcceptThreadPool*, int);
  struct NETServer* server;
  pthread_mutex_t mutex;
  _Atomic uint32_t amount;
  _Atomic uint32_t amount2;
  uint32_t growth;
};

extern int InitAcceptThreadPool(struct NETAcceptThreadPool* const, const uint32_t, const uint32_t);

extern void FreeAcceptThreadPool(struct NETAcceptThreadPool* const);

#ifdef __cplusplus
}
#endif

#endif // W_pOOj__2VkXM_ew9zpRf2pZH_H8ggTe