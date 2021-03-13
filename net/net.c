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

#include "net.h"

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
#include <netinet/tcp.h>

#include <stdio.h>
#include <arpa/inet.h> // htons, htonl, etc

void SocketNoBlock(const int sfd) {
  int err = fcntl(sfd, F_GETFL, 0);
  if(err == -1) {
    err = 0;
  }
  (void) fcntl(sfd, F_SETFL, err | O_NONBLOCK);
}

void SocketCorkOn(const int sfd) {
  (void) setsockopt(sfd, SOL_TCP, TCP_CORK, &(int){1}, sizeof(int));
}

void SocketCorkOff(const int sfd) {
  (void) setsockopt(sfd, SOL_TCP, TCP_CORK, &(int){0}, sizeof(int));
}

void SocketNoDelayOn(const int sfd) {
  (void) setsockopt(sfd, IPPROTO_TCP, TCP_NODELAY, &(int){1}, sizeof(int));
}

void SocketNoDelayOff(const int sfd) {
  (void) setsockopt(sfd, IPPROTO_TCP, TCP_NODELAY, &(int){0}, sizeof(int));
}

void GetIPAsString(const struct NETSocket socket, char* str) {
  if(socket.addrlen == 4) {
    strcpy(str, inet_ntoa(((struct sockaddr_in*)&socket.addr)->sin_addr));
  }
  printf("addrlen: %d\n", socket.addrlen);
}

void FreeSocket(struct NETSocket* const socket) {
  if(socket->send_buffer != NULL) {
    free(socket->send_buffer);
    socket->send_buffer = NULL;
  }
  socket->length = 0;
  close(socket->sfd);
}

int TCPSend(struct NETSocket* restrict socket, void* const buffer, const size_t length) {
  void* ptr;
  long bytes = send(socket->sfd, buffer, length, MSG_NOSIGNAL);
  puts("TCPSend()");
  if(bytes == -1) {
    if(errno == EAGAIN || errno == EWOULDBLOCK) {
      puts("not everything sent");
      ptr = realloc(socket->send_buffer, socket->length + length - bytes);
      if(ptr == NULL) {
        return ENOMEM;
      }
      socket->send_buffer = ptr;
      (void) memcpy(socket->send_buffer + socket->length, buffer + bytes, length - bytes);
      socket->length += length - bytes;
      return 0;
    } else {
      if(errno == EPIPE && socket->state != NET_CLOSED) {
        puts("connection shut down unexpectedly");
        socket->state = NET_CLOSED;
        if(socket->onclose != NULL) {
          socket->onclose(*socket);
        }
        FreeSocket(socket);
      }
      return errno;
    }
  } else if(bytes == length) {
    puts("everything sent");
    return 0;
  } else {
    puts("sent lower amount than requested but no error");
    exit(1);
  }
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
    arra->arr[i].handler(res, GetAddrInfo(arra->arr[i].hostname, arra->arr[i].service, arra->arr[i].flags, &res));
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
  struct NETSocket* sock;
  int err;
  int i;
  int temp;
  long bytes;
  uint8_t byte[1];
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
      sock = net_avl_search(&manager->avl_tree, events[i].data.fd);
      if((events[i].events & EPOLLHUP) != 0) { // they can message us but we can't message them
        if(sock->state == NET_OPEN) {
          sock->state = NET_SEND_CLOSING;
          puts("the peer doesn't accept messages anymore, connection closing");
        } else if(sock->state != NET_CLOSED && sock->state != NET_SEND_CLOSING) {
          puts("the peer doesn't accept messages anymore, connection closed");
          sock->state = NET_CLOSED;
          if(sock->onclose != NULL) {
            sock->onclose(*sock);
          }
          FreeSocket(sock);
          continue;
        }
      }
      if((events[i].events & EPOLLRDHUP) != 0) { // they can't message us but we can message them
        if(sock->state == NET_OPEN) {
          sock->state = NET_RECEIVE_CLOSING;
          puts("the peer won't send messages anymore, connection closing");
        } else if(sock->state != NET_CLOSED && sock->state != NET_RECEIVE_CLOSING) {
          puts("the peer won't send messages anymore, connection closed");
          sock->state = NET_CLOSED;
          if(sock->onclose != NULL) {
            sock->onclose(*sock);
          }
          FreeSocket(sock);
          continue;
        }
      }
      if((events[i].events & EPOLLERR) != 0) { // an event
        getsockopt(sock->sfd, SOL_SOCKET, SO_ERROR, &temp, NULL);
        printf("got an event, code %d, message: %s\n", temp, strerror(temp));
      }
      if((events[i].events & EPOLLIN) != 0) { // can receive
        bytes = recv(sock->sfd, byte, 1, MSG_PEEK);
        if(bytes == 0) {
          if(sock->state != NET_CLOSED) {
            puts("connection closed, received 0 bytes");
            sock->state = NET_CLOSED;
            if(sock->onclose != NULL) {
              sock->onclose(*sock);
            }
            FreeSocket(sock);
            continue;
          }
        } else if(sock->onmessage != NULL) {
          sock->onmessage(*sock);
        }
      }
      if(sock->length != 0 && sock->state != NET_SEND_CLOSING && (events[i].events & EPOLLOUT) != 0) { // can send
        puts("sending what we have in buffer");
        //char buf[] = "GET /index.html HTTP/1.1\r\n";
        bytes = send(sock->sfd, sock->send_buffer, sock->length, MSG_NOSIGNAL);
        if(bytes == -1) {
          if(errno == EPIPE && sock->state != NET_CLOSED) {
            puts("connection shut down unexpectedly");
            sock->state = NET_CLOSED;
            if(sock->onclose != NULL) {
              sock->onclose(*sock);
            }
            FreeSocket(sock);
            continue;
          }
          sock->onerror(*sock);
        } else {
          sock->length -= bytes;
          if(sock->length == 0) {
            free(sock->send_buffer);
            sock->send_buffer = NULL;
            if(sock->onsent != NULL) {
              sock->onsent(*sock);
            }
          }
        }
      }
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

int AddSocket(struct NETConnectionManager* const manager, const struct NETSocket socket) {
  int err = net_avl_insert(&manager->avl_tree, socket);
  if(err != 0) {
    puts("failed to add a socket");
    return err;
  }
  err = epoll_ctl(manager->epoll, EPOLL_CTL_ADD, socket.sfd, &((struct epoll_event) {
    .events = EPOLLIN | EPOLLOUT | EPOLLRDHUP,
    .data = (epoll_data_t) {
      .fd = socket.sfd
    }
  }));
  if(err != 0) {
    net_avl_delete(&manager->avl_tree, socket.sfd);
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
    sfd = socket(n->ai_family, n->ai_socktype, n->ai_protocol);
    if(sfd == -1) {
      printf("creating a socket failed with reason: %s\n", strerror(errno));
      continue;
    }
    printf("we connecting at   %ld\n", GetTime(0));
    printf("ip: %s\n", inet_ntoa(((struct sockaddr_in*) n->ai_addr)->sin_addr));
    err = connect(sfd, n->ai_addr, n->ai_addrlen);
    printf("done connecting at %ld\n", GetTime(0));
    if(err != 0) {
      (void) close(sfd);
      continue;
    } else {
      sockt->addr = *n->ai_addr;
      sockt->send_buffer = 0;
      sockt->length = 0;
      sockt->addrlen = n->ai_addrlen;
      sockt->state = NET_OPEN;
      sockt->flags = n->ai_flags;
      sockt->family = n->ai_family;
      sockt->socktype = n->ai_socktype;
      sockt->protocol = n->ai_protocol;
      sockt->sfd = sfd;
      freeaddrinfo(res);
      return 0;
    }
  }
  freeaddrinfo(res);
  return -1;
}

int SyncTCP_GAIConnect(const char* const hostname, const char* const service, const int which_ip, struct NETSocket* restrict socket) {
  struct addrinfo* res;
  int err = GetAddrInfo(hostname, service, which_ip, &res);
  if(err != 0) {
    return err;
  }
  return SyncTCPConnect(res, socket);
}

int SyncTCP_IP_GAIConnect(const char* const hostname, const char* const service, const int which_ip, struct NETSocket* restrict socket) {
  struct addrinfo* res;
  int err = GetAddrInfo(hostname, service, AI_NUMERICHOST | which_ip, &res);
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
      sockt->addr = *n->ai_addr;
      sockt->send_buffer = 0;
      sockt->length = 0;
      sockt->addrlen = n->ai_addrlen;
      sockt->state = NET_OPEN;
      sockt->flags = n->ai_flags;
      sockt->family = n->ai_family;
      sockt->socktype = n->ai_socktype;
      sockt->protocol = n->ai_protocol;
      sockt->sfd = sfd;
      freeaddrinfo(res);
      return 0;
    }
  }
  freeaddrinfo(res);
  return -1;
}

int SyncTCP_GAIListen(const char* const hostname, const char* const service, const int which_ip, struct NETSocket* restrict socket) {
  struct addrinfo* res;
  int err = GetAddrInfo(hostname, service, AI_PASSIVE | which_ip, &res);
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
    sfd = socket(n->ai_family, n->ai_socktype, n->ai_protocol);
    if(sfd == -1) {
      printf("creating a socket failed with reason: %s\n", strerror(errno));
      continue;
    }
    puts("got socket");
    nets = (struct NETSocket) {
      .addr = *n->ai_addr,
      .onmessage = NULL,
      .onclose = NULL,
      .onerror = NULL,
      .onsent = NULL,
      .send_buffer = 0,
      .length = 0,
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
      arr->handler(&nets, -1);
      if(n->ai_next == NULL) {
        arr->handler(&nets, -2);
        break;
      }
      continue;
    } else {
      nets.state = NET_OPEN;
      arr->handler(&nets, sfd);
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
    sfd = socket(n->ai_family, n->ai_socktype, n->ai_protocol);
    if(sfd == -1) {
      printf("creating a socket failed with reason: %s\n", strerror(errno));
      continue;
    }
    puts("got socket");
    nets = (struct NETSocket) {
      .addr = *n->ai_addr,
      .onmessage = NULL,
      .onclose = NULL,
      .onerror = NULL,
      .onsent = NULL,
      .send_buffer = 0,
      .length = 0,
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
      arr->handler(&nets, -1);
      continue;
    }
    puts("bind succeeded");
    err = listen(sfd, 64);
    if(err != 0) {
      (void) close(sfd);
      arr->handler(&nets, -2);
      if(n->ai_next == NULL) {
        arr->handler(&nets, -3);
        break;
      }
      continue;
    } else {
      nets.state = NET_OPEN;
      arr->handler(&nets, sfd);
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
