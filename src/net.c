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

#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <pthread.h>
#include <sys/epoll.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/syscall.h>

#include "net.h"
#include "net_avl.h"

#include <stdio.h>
#include <arpa/inet.h> // htons, htonl, etc

void SocketNoBlock(const int sfd) {
  int err = fcntl(sfd, F_GETFL, 0);
  if(err == -1) {
    err = 0;
  }
  (void) fcntl(sfd, F_SETFL, err | O_NONBLOCK);
}

int GetAddrInfo(const char* const hostname, const char* const service, const int flags, struct addrinfo** const restrict res) {
  return getaddrinfo(hostname, service, &((struct addrinfo) {
    .ai_family = flags & (AF_INET | AF_INET6 | AF_UNSPEC),
    .ai_socktype = SOCK_STREAM,
    .ai_protocol = IPPROTO_TCP,
    .ai_flags = AI_V4MAPPED | AI_NUMERICSERV | (flags & (AI_NUMERICHOST | AI_PASSIVE)),
    .ai_addrlen = 0,
    .ai_addr = NULL,
    .ai_next = NULL,
    .ai_canonname = NULL
  }), res);
}

#define arra ((struct ANET_GAIArray*) a)

__nonnull((1))
static void* AsyncGAIThread(void* a) {
  struct addrinfo* res;
  uint_fast32_t i = 0;
  for(; i < arra->count; ++i) {
    arra->arr[i].callback(res, GetAddrInfo(arra->arr[i].hostname, arra->arr[i].service, arra->arr[i].flags, &res));
    freeaddrinfo(res);
  }
  return NULL;
}

#undef arra

int AsyncGetAddrInfo(struct ANET_GAIArray* const array) {
  pthread_t t;
  return pthread_create(&t, NULL, AsyncGAIThread, array);
}

static void dummy_signal_handler(__unused int sig) {}

#define manager ((struct NETConnectionManager*) s)

__nothrow __nonnull((1))
static void EPollThreadCleanup(void* s) {
  close(manager->epoll);
  net_avl_free(&manager->avl_tree);
}

__nonnull((1))
static void* EPollThread(void* s) {
  sigset_t mask;
  struct epoll_event events[100];
  int err;
  int i;
  sigemptyset(&mask);
  sigfillset(&mask);
  (void) pthread_sigmask(SIG_BLOCK, &mask, NULL);
  sigemptyset(&mask);
  sigaddset(&mask, SIGRTMAX);
  (void) pthread_sigmask(SIG_UNBLOCK, &mask, NULL);
  (void) sigaction(SIGRTMAX, &((struct sigaction) {
    .sa_flags = 0,
    .sa_handler = dummy_signal_handler
  }), NULL);
  pthread_cleanup_push(EPollThreadCleanup, s);
  while(1) {
    err = epoll_wait(manager->epoll, events, 100, -1);
    if(err == -1) {
      close(manager->epoll);
      net_avl_free(&manager->avl_tree);
      return NULL;
    }
    for(i = 0; i < err; ++i) {
      net_avl_search(&manager->avl_tree, events[i].data.fd, events[i].events);
    }
  }
  pthread_cleanup_pop(1);
  return NULL;
}

#undef manager

int InitConnectionManager(struct NETConnectionManager* const manager, const uint32_t avg_size) {
  int epoll = epoll_create1(0);
  if(epoll == -1) {
    return errno;
  }
  int err = pthread_create(&manager->thread, NULL, EPollThread, manager);
  if(err != 0) {
    close(epoll);
    return err;
  }
  manager->avl_tree = net_avl_tree(avg_size);
  err = net_avl_init(&manager->avl_tree);
  if(err != 0) {
    close(epoll);
  } else {
    manager->epoll = epoll;
  }
  return err;
}

int AddSocket(struct NETConnectionManager* const manager, const int sfd, void (*callback)(int, uint32_t)) {
  int err = net_avl_insert(&manager->avl_tree, sfd, callback);
  if(err != 0) {
    puts("failed to add a socket");
    return err;
  }
  err = epoll_ctl(manager->epoll, EPOLL_CTL_ADD, sfd, &((struct epoll_event) {
    .events = EPOLLIN | EPOLLRDHUP,
    .data = (epoll_data_t) {
      .fd = sfd
    }
  }));
  if(err != 0) {
    net_avl_delete(&manager->avl_tree, sfd);
    return errno;
  }
  return 0;
}

int DeleteSocket(struct NETConnectionManager* const manager, const int sfd) {
  net_avl_delete(&manager->avl_tree, sfd);
  int err = epoll_ctl(manager->epoll, EPOLL_CTL_DEL, sfd, (struct epoll_event*) manager);
  if(err != 0) {
    return errno;
  }
  return 0;
}

void FreeConnectionManager(struct NETConnectionManager* const manager) {
  (void) pthread_cancel(manager->thread);
  (void) pthread_sigqueue(manager->thread, SIGRTMAX, (union sigval) { .sival_ptr = NULL });
}

int SyncTCPConnect(struct addrinfo* const res, struct NETSocket* restrict sockt) {
  struct addrinfo* n;
  int sfd;
  int err;
  for(n = res; n != NULL; n = n->ai_next) {
    printf("ip: %s\n", inet_ntoa(((struct sockaddr_in*) n->ai_addr)->sin_addr));
    sfd = socket(n->ai_family, n->ai_socktype, n->ai_protocol);
    if(sfd == -1) {
      printf("creating a socket failed with reason: %s\n", strerror(errno));
      continue;
    }
    printf("we connecting at   %ld\n", GetTime(0));
    err = connect(sfd, n->ai_addr, n->ai_addrlen);
    printf("done connecting at %ld\n", GetTime(0));
    if(err != 0) {
      (void) close(sfd);
      continue;
    } else {
      *sockt = (struct NETSocket) {
        .addr = *n->ai_addr,
        .canonname = n->ai_canonname,
        .addrlen = n->ai_addrlen,
        .state = NET_OPEN,
        .flags = n->ai_flags,
        .family = n->ai_family,
        .socktype = n->ai_socktype,
        .protocol = n->ai_protocol,
        .sfd = sfd
      };
      freeaddrinfo(res);
      return 0;
    }
  }
  freeaddrinfo(res);
  return -1;
}

int SyncTCP_GAIConnect(const char* const hostname, const char* const service, struct NETSocket* restrict socket) {
  struct addrinfo* res;
  int err = GetAddrInfo(hostname, service, 0, &res);
  if(err != 0) {
    return err;
  }
  return SyncTCPConnect(res, socket);
}

int SyncTCP_IP_GAIConnect(const char* const hostname, const char* const service, struct NETSocket* restrict socket) {
  struct addrinfo* res;
  int err = GetAddrInfo(hostname, service, AI_NUMERICHOST, &res);
  if(err != 0) {
    return err;
  }
  return SyncTCPConnect(res, socket);
}

int SyncTCPListen(struct addrinfo* const res, struct NETSocket* restrict sockt) {
  struct addrinfo* n;
  int sfd;
  int err;
  for(n = res; n != NULL; n = n->ai_next) {
    printf("ip: %s\n", inet_ntoa(((struct sockaddr_in*) n->ai_addr)->sin_addr));
    sfd = socket(n->ai_family, n->ai_socktype, n->ai_protocol);
    if(sfd == -1) {
      printf("creating a socket failed with reason: %s\n", strerror(errno));
      continue;
    }
    (void) setsockopt(sfd, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int));
    err = bind(sfd, n->ai_addr, n->ai_addrlen);
    if(err != 0) {
      (void) close(sfd);
      continue;
    }
    err = listen(sfd, 64);
    if(err != 0) {
      (void) close(sfd);
      continue;
    } else {
      *sockt = (struct NETSocket) {
        .addr = *n->ai_addr,
        .canonname = n->ai_canonname,
        .addrlen = n->ai_addrlen,
        .state = NET_OPEN,
        .flags = n->ai_flags,
        .family = n->ai_family,
        .socktype = n->ai_socktype,
        .protocol = n->ai_protocol,
        .sfd = sfd
      };
      freeaddrinfo(res);
      return 0;
    }
  }
  freeaddrinfo(res);
  return -1;
}

int SyncTCP_GAIListen(const char* const service, struct NETSocket* restrict socket) {
  struct addrinfo* res;
  int err = GetAddrInfo(NULL, service, AI_PASSIVE, &res);
  if(err != 0) {
    return err;
  }
  return SyncTCPListen(res, socket);
}

#define arr ((struct ANET*) a)

__nonnull((1))
static void* AsyncTCPConnectThread(void* a) {
  struct addrinfo* n;
  struct NETSocket nets;
  int err;
  int sfd;
  /*
  >0 = success
  -1 = connect err
  -2 = no socket succeeded
  */
  for(n = arr->addrinfo; n != NULL; n = n->ai_next) {
    printf("ip: %s\n", inet_ntoa(((struct sockaddr_in*) n->ai_addr)->sin_addr));
    sfd = socket(n->ai_family, n->ai_socktype, n->ai_protocol);
    if(sfd == -1) {
      printf("creating a socket failed with reason: %s\n", strerror(errno));
      continue;
    }
    puts("got socket");
    nets = (struct NETSocket) {
      .addr = *n->ai_addr,
      .canonname = n->ai_canonname,
      .addrlen = n->ai_addrlen,
      .state = NET_CLOSED,
      .flags = n->ai_flags,
      .family = n->ai_family,
      .socktype = n->ai_socktype,
      .protocol = n->ai_protocol,
      .sfd = sfd
    };
    printf("we connecting at %ld\n", GetTime(0));
    err = connect(sfd, n->ai_addr, n->ai_addrlen);
    printf("done connecting at %ld\n", GetTime(0));
    if(err != 0) {
      (void) close(sfd);
      arr->callback(&nets, -1);
      if(n->ai_next == NULL) {
        arr->callback(&nets, -2);
        break;
      }
      continue;
    } else {
      nets.state = NET_OPEN;
      arr->callback(&nets, sfd);
      break;
    }
  }
  freeaddrinfo(arr->addrinfo);
  return NULL;
}

__nonnull((1))
static void* AsyncTCPListenThread(void* a) {
  struct addrinfo* n;
  struct NETSocket nets;
  int err;
  int sfd;
  /*
  >0 = success
  -1 = bind err
  -2 = listen err
  -3 = no socket succeeded
  */
  for(n = arr->addrinfo; n != NULL; n = n->ai_next) {
    printf("ip: %s\n", inet_ntoa(((struct sockaddr_in*) n->ai_addr)->sin_addr));
    sfd = socket(n->ai_family, n->ai_socktype, n->ai_protocol);
    if(sfd == -1) {
      printf("creating a socket failed with reason: %s\n", strerror(errno));
      continue;
    }
    puts("got socket");
    nets = (struct NETSocket) {
      .addr = *n->ai_addr,
      .canonname = n->ai_canonname,
      .addrlen = n->ai_addrlen,
      .state = NET_CLOSED,
      .flags = n->ai_flags,
      .family = n->ai_family,
      .socktype = n->ai_socktype,
      .protocol = n->ai_protocol,
      .sfd = sfd
    };
    (void) setsockopt(sfd, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int));
    err = bind(sfd, n->ai_addr, n->ai_addrlen);
    if(err != 0) {
      (void) close(sfd);
      arr->callback(&nets, -1);
      continue;
    }
    puts("bind succeeded");
    err = listen(sfd, 64);
    if(err != 0) {
      (void) close(sfd);
      arr->callback(&nets, -2);
      if(n->ai_next == NULL) {
        arr->callback(&nets, -3);
        break;
      }
      continue;
    } else {
      nets.state = NET_OPEN;
      arr->callback(&nets, sfd);
      break;
    }
  }
  freeaddrinfo(arr->addrinfo);
  return NULL;
}

#undef arr

int AsyncTCPConnect(struct ANET* const info) {
  pthread_t t;
  return pthread_create(&t, NULL, AsyncTCPConnectThread, info);
}

int AsyncTCPListen(struct ANET* const info) {
  pthread_t t;
  return pthread_create(&t, NULL, AsyncTCPListenThread, info);
}

int TCPSend(const int sfd, const void* const buffer, const size_t length, const int cork) {
  int err = send(sfd, buffer, length, MSG_NOSIGNAL | (cork * MSG_MORE));
}
