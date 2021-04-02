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
#include <stdlib.h>
#include <sys/epoll.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/syscall.h>
#include <netinet/tcp.h>

#include <stdio.h>
#include <arpa/inet.h> // htons, htonl, etc

void TCPNoBlock(const int sfd) {
  int err = fcntl(sfd, F_GETFL, 0);
  if(err == -1) {
    err = 0;
  }
  (void) fcntl(sfd, F_SETFL, err | O_NONBLOCK);
}

void TCPCorkOn(const int sfd) {
  (void) setsockopt(sfd, IPPROTO_TCP, TCP_CORK, &(int){1}, sizeof(int));
}

void TCPCorkOff(const int sfd) {
  (void) setsockopt(sfd, IPPROTO_TCP, TCP_CORK, &(int){0}, sizeof(int));
}

void TCPNoDelayOn(const int sfd) {
  (void) setsockopt(sfd, IPPROTO_TCP, TCP_NODELAY, &(int){1}, sizeof(int));
}

void TCPNoDelayOff(const int sfd) {
  (void) setsockopt(sfd, IPPROTO_TCP, TCP_NODELAY, &(int){0}, sizeof(int));
}

void TCPIdleTimeout(const int sfd, const int seconds) {
  (void) setsockopt(sfd, IPPROTO_TCP, TCP_KEEPIDLE, &seconds, sizeof(int));
}

void TCPSocketFree(struct NETSocket* const socket) {
  if(socket->send_buffer != NULL) {
    free(socket->send_buffer);
    socket->send_buffer = NULL;
    socket->send_length = 0;
  }
  (void) close(socket->sfd);
}

void TCPServerFree(struct NETServer* const server) {
  if(server->connections != NULL) {
    for(uint32_t i = 0; i < server->conn_count; ++i) {
      TCPShutdown(GetSocket(server->manager, server->connections[i]));
    }
    free(server->connections);
    server->connections = NULL;
    server->conn_count = 0;
    server->max_conn_count = 0;
  }
  (void) close(server->sfd);
}

static void TCPKill(struct NETSocket* const socket) {
  socket->state = NET_CLOSED;
  if(socket->onclose != NULL) {
    socket->onclose(socket);
  }
  TCPSocketFree(socket);
}

int TCPSend(struct NETSocket* const socket, void* const buffer, const ssize_t length, struct NETConnManager* const manager) {
  puts("TCPSend()");
  ssize_t bytes = send(socket->sfd, buffer, length, MSG_NOSIGNAL);
  if(bytes == -1) {
    if(errno == EAGAIN) {
      puts("not everything sent");
      void* const ptr = realloc(socket->send_buffer, socket->send_length + length - bytes);
      if(ptr == NULL) {
        return ENOMEM;
      }
      socket->send_buffer = ptr;
      (void) memcpy((char*) socket->send_buffer + socket->send_length, (char*) buffer + bytes, length - bytes);
      socket->send_length += length - bytes;
      if(manager != NULL) {
        epoll_ctl(manager->epoll, EPOLL_CTL_MOD, socket->sfd, &((struct epoll_event) {
          .events = EPOLLIN | EPOLLRDHUP,
          .data = (epoll_data_t) {
            .fd = socket->sfd
          }
        }));
      }
    } else {
      if(errno == EPIPE && socket->state != NET_CLOSED) {
        puts("connection shut down unexpectedly");
        TCPKill(socket);
        DeleteEventlessSocket(manager, socket->sfd);
      }
      return errno;
    }
  } else if(bytes == length) {
    puts("everything sent");
  } else {
    puts("sent lower amount than requested but no error");
    exit(1);
  }
  return 0;
}

void TCPSendShutdown(struct NETSocket* const socket) {
  if(socket->state == NET_OPEN) {
    socket->state = NET_SEND_CLOSING;
    shutdown(socket->sfd, SHUT_WR);
  }
}

void TCPReceiveShutdown(struct NETSocket* const socket) {
  if(socket->state == NET_OPEN) {
    socket->state = NET_RECEIVE_CLOSING;
    shutdown(socket->sfd, SHUT_RD);
  }
}

void TCPShutdown(struct NETSocket* const socket) {
  if(socket->state != NET_CLOSED) {
    shutdown(socket->sfd, SHUT_RDWR);
  }
}

int GetAddrInfo(const char* const hostname, const char* const service, const int flags, struct addrinfo** const res) {
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

static void* AsyncGAIThread(void* a) {
  struct addrinfo* res;
  for(uint_fast32_t i = 0; i < arra->count; ++i) {
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

int SyncTCPConnect(struct addrinfo* const res, struct NETSocket* const sock) {
  sock->onmessage = NULL;
  sock->onclose = NULL;
  sock->onerror = NULL;
  sock->onsent = NULL;
  sock->send_buffer = NULL;
  sock->send_length = 0;
  for(struct addrinfo* n = res; n != NULL; n = n->ai_next) {
    sock->addr = *n->ai_addr;
    sock->addrlen = n->ai_addrlen;
    sock->flags = n->ai_flags;
    sock->family = n->ai_family;
    sock->socktype = n->ai_socktype;
    sock->protocol = n->ai_protocol;
    sock->sfd = -1;
    int sfd = socket(n->ai_family, n->ai_socktype, n->ai_protocol);
    if(sfd == -1) {
      printf("creating a socket failed with reason: %s\n", strerror(errno));
      continue;
    }
    printf("addrlen: %d\n", sock->addrlen);
    sock->sfd = sfd;
    uint64_t time = GetTime(0);
    printf("we connecting at   %ld\n", time);
    printf("ip: %s\n", inet_ntoa(((struct sockaddr_in*) n->ai_addr)->sin_addr));
    int err = connect(sfd, n->ai_addr, n->ai_addrlen);
    uint64_t time2 = GetTime(0);
    printf("done connecting at %ld (took %f ms)\n", time2, (float)(time2 - time) / 1000000);
    if(err != 0) {
      (void) close(sfd);
      continue;
    } else {
      sock->state = NET_OPEN;
      freeaddrinfo(res);
      return 0;
    }
  }
  freeaddrinfo(res);
  return -1;
}

int SyncTCP_GAIConnect(const char* const hostname, const char* const service, const int which_ip, struct NETSocket* const socket) {
  struct addrinfo* res;
  int err = GetAddrInfo(hostname, service, which_ip, &res);
  if(err != 0) {
    return err;
  }
  return SyncTCPConnect(res, socket);
}

int SyncTCP_IP_GAIConnect(const char* const hostname, const char* const service, const int which_ip, struct NETSocket* const socket) {
  struct addrinfo* res;
  int err = GetAddrInfo(hostname, service, AI_NUMERICHOST | which_ip, &res);
  if(err != 0) {
    return err;
  }
  return SyncTCPConnect(res, socket);
}

int SyncTCPListen(struct addrinfo* const res, struct NETServer* const serv) {
  serv->onconnection = NULL;
  serv->pool = NULL;
  serv->onerror = NULL;
  serv->manager = NULL;
  serv->connections = NULL;
  serv->conn_count = 0;
  serv->max_conn_count = 0;
  for(struct addrinfo* n = res; n != NULL; n = n->ai_next) {
    serv->addr = *n->ai_addr;
    serv->addrlen = n->ai_addrlen;
    serv->flags = n->ai_flags;
    serv->family = n->ai_family;
    serv->socktype = n->ai_socktype;
    serv->protocol = n->ai_protocol;
    serv->sfd = -1;
    int sfd = socket(n->ai_family, n->ai_socktype, n->ai_protocol);
    if(sfd == -1) {
      printf("creating a socket failed with reason: %s\n", strerror(errno));
      continue;
    }
    serv->sfd = sfd;
    (void) setsockopt(sfd, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int));
    int err = bind(sfd, n->ai_addr, n->ai_addrlen);
    if(err != 0) {
      (void) close(sfd);
      continue;
    }
    err = listen(sfd, 64);
    if(err != 0) {
      (void) close(sfd);
      continue;
    } else {
      freeaddrinfo(res);
      return 0;
    }
  }
  freeaddrinfo(res);
  return -1;
}

int SyncTCP_GAIListen(const char* const hostname, const char* const service, const int which_ip, struct NETServer* const server) {
  struct addrinfo* res;
  int err = GetAddrInfo(hostname, service, AI_PASSIVE | which_ip, &res);
  if(err != 0) {
    return err;
  }
  return SyncTCPListen(res, server);
}

#define arr ((struct ANET_C*) a)

static void* AsyncTCPConnectThread(void* a) {
  /*
  >0 = success
  -1 = connect err
  -2 = no socket succeeded
  */
  struct NETSocket sock;
  sock.onmessage = NULL;
  sock.onclose = NULL;
  sock.onerror = NULL;
  sock.onsent = NULL;
  sock.send_buffer = 0;
  sock.send_length = 0;
  for(struct addrinfo* n = arr->addrinfo; n != NULL; n = n->ai_next) {
    sock.addr = *n->ai_addr;
    sock.addrlen = n->ai_addrlen;
    sock.flags = n->ai_flags;
    sock.family = n->ai_family;
    sock.socktype = n->ai_socktype;
    sock.protocol = n->ai_protocol;
    sock.sfd = -1;
    int sfd = socket(n->ai_family, n->ai_socktype, n->ai_protocol);
    if(sfd == -1) {
      printf("creating a socket failed with reason: %s\n", strerror(errno));
      if(n->ai_next == NULL) {
        arr->handler(&sock, -2);
        break;
      }
      continue;
    }
    sock.sfd = sfd;
    puts("got socket");
    uint64_t time = GetTime(0);
    printf("we connecting at   %ld\n", time);
    int err = connect(sfd, n->ai_addr, n->ai_addrlen);
    uint64_t time2 = GetTime(0);
    printf("done connecting at %ld (took %f ms)\n", time2, (float)(time2 - time) / 1000000);
    if(err != 0) {
      (void) close(sfd);
      arr->handler(&sock, -1);
      if(n->ai_next == NULL) {
        arr->handler(NULL, -2);
        break;
      }
      continue;
    } else {
      sock.state = NET_OPEN;
      arr->handler(&sock, sfd);
      break;
    }
  }
  freeaddrinfo(arr->addrinfo);
  return NULL;
}

#undef arr
#define arr ((struct ANET_L*) a)

static void* AsyncTCPListenThread(void* a) {
  /*
  >0 = success
  -1 = bind err
  -2 = listen err
  -3 = no socket succeeded
  */
  struct NETServer serv;
  serv.onconnection = NULL;
  serv.pool = NULL;
  serv.onerror = NULL;
  serv.manager = NULL;
  serv.connections = NULL;
  serv.conn_count = 0;
  serv.max_conn_count = 0;
  for(struct addrinfo* n = arr->addrinfo; n != NULL; n = n->ai_next) {
    serv.addr = *n->ai_addr;
    serv.addrlen = n->ai_addrlen;
    serv.flags = n->ai_flags;
    serv.family = n->ai_family;
    serv.socktype = n->ai_socktype;
    serv.protocol = n->ai_protocol;
    serv.sfd = -1;
    int sfd = socket(n->ai_family, n->ai_socktype, n->ai_protocol);
    if(sfd == -1) {
      printf("creating a socket failed with reason: %s\n", strerror(errno));
      if(n->ai_next == NULL) {
        arr->handler(&serv, -3);
        break;
      }
      continue;
    }
    serv.sfd = sfd;
    puts("got socket");
    (void) setsockopt(sfd, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int));
    int err = bind(sfd, n->ai_addr, n->ai_addrlen);
    if(err != 0) {
      (void) close(sfd);
      arr->handler(&serv, -1);
      continue;
    }
    err = listen(sfd, 64);
    if(err != 0) {
      (void) close(sfd);
      arr->handler(&serv, -2);
      if(n->ai_next == NULL) {
        arr->handler(NULL, -3);
        break;
      }
      continue;
    } else {
      arr->handler(&serv, sfd);
      break;
    }
  }
  freeaddrinfo(arr->addrinfo);
  return NULL;
}

#undef arr

int AsyncTCPConnect(struct ANET_C* const info) {
  pthread_t t;
  return pthread_create(&t, NULL, AsyncTCPConnectThread, info);
}

int AsyncTCPListen(struct ANET_L* const info) {
  pthread_t t;
  return pthread_create(&t, NULL, AsyncTCPListenThread, info);
}

#define MSG_BUFFER_LEN 16384

static int EPollSocketGetMessage(struct NETConnManager* const manager, struct NETSocket* const socket, uint8_t* const mem, const size_t length) {
  ssize_t bytes = recv(socket->sfd, mem, length, MSG_PEEK);
  if(bytes == 0) {
    if(socket->state != NET_CLOSED) {
      puts("connection closed, received 0 bytes");
      TCPKill(socket);
      DeleteEventlessSocket(manager, socket->sfd);
    }
    return -1;
  } else if(bytes == -1) {
    if((errno == ECONNRESET || errno == ETIMEDOUT) && socket->state != NET_CLOSED) {
      puts("connection reset or timedout while reading data");
      TCPKill(socket);
      DeleteEventlessSocket(manager, socket->sfd);
    } else if(errno != EAGAIN) {
      printf("recv error %d | %s\n", errno, strerror(errno));
      socket->onerror(socket);
    }
    return -1;
  } else if(socket->onmessage != NULL) {
    socket->onmessage(socket);
  } else {
    while(EPollSocketGetMessage(manager, socket, mem, MSG_BUFFER_LEN) == 0);
  }
  return 0;
}

#define manager ((struct NETConnManager*) info->si_value.sival_ptr)

static void EPollThreadHandler(int sig, siginfo_t* info, void* ucontext) {
  (void) close(manager->epoll);
  net_avl_multithread_free(&manager->tree);
}

#undef manager
#define manager ((struct NETConnManager*) s)

static void* EPollThread(void* s) {
  sigset_t mask;
  (void) sigfillset(&mask);
  (void) sigdelset(&mask, SIGRTMAX - 1);
  (void) pthread_sigmask(SIG_SETMASK, &mask, NULL);
  (void) sigemptyset(&mask);
  (void) sigaddset(&mask, SIGRTMAX - 1);
  (void) sigaction(SIGRTMAX - 1, &((struct sigaction) {
    .sa_flags = SA_SIGINFO,
    .sa_sigaction = EPollThreadHandler
  }), NULL);
  struct epoll_event events[100];
  uint8_t mem[MSG_BUFFER_LEN];
  while(1) {
    int count = epoll_wait(manager->epoll, events, 100, -1);
    (void) pthread_sigmask(SIG_BLOCK, &mask, NULL);
    if(count == -1) {
      (void) close(manager->epoll);
      net_avl_multithread_free(&manager->tree);
    }
    for(int i = 0; i < count; ++i) {
      struct NETSocket* sock = net_avl_multithread_search(&manager->tree, events[i].data.fd);
      if((sock->flags & AI_PASSIVE) == 0) {
        if((events[i].events & EPOLLHUP) != 0) {
          if(sock->state == NET_OPEN) {
            sock->state = NET_SEND_CLOSING;
            puts("the peer doesn't accept messages anymore, connection closing");
          } else if(sock->state != NET_CLOSED && sock->state != NET_SEND_CLOSING) {
            puts("the peer doesn't accept messages anymore, connection closed");
            TCPKill(sock);
            DeleteEventlessSocket(manager, sock->sfd);
            continue;
          }
        }
        if((events[i].events & EPOLLRDHUP) != 0) {
          if(sock->state == NET_OPEN) {
            sock->state = NET_RECEIVE_CLOSING;
            puts("the peer won't send messages anymore, connection closing");
          } else if(sock->state != NET_CLOSED && sock->state != NET_RECEIVE_CLOSING) {
            puts("the peer won't send messages anymore, connection closed");
            TCPKill(sock);
            DeleteEventlessSocket(manager, sock->sfd);
            continue;
          }
        }
        if((events[i].events & EPOLLERR) != 0) {
          int temp;
          getsockopt(sock->sfd, SOL_SOCKET, SO_ERROR, &temp, NULL);
          printf("got an event, code %d, message: %s\n", temp, strerror(temp));
        }
        if((events[i].events & EPOLLIN) != 0) {
          (void) EPollSocketGetMessage(manager, sock, mem, 1);
        }
        if(sock->send_length != 0 && sock->state != NET_SEND_CLOSING && (events[i].events & EPOLLOUT) != 0) {
          puts("sending what we have in buffer");
          ssize_t bytes = send(sock->sfd, sock->send_buffer, sock->send_length, MSG_NOSIGNAL);
          if(bytes == -1) {
            if(errno == EPIPE && sock->state != NET_CLOSED) {
              puts("connection shut down unexpectedly");
              TCPKill(sock);
              DeleteEventlessSocket(manager, sock->sfd);
              continue;
            }
            sock->onerror(sock);
          } else {
            sock->send_length -= bytes;
            if(sock->send_length == 0) {
              free(sock->send_buffer);
              sock->send_buffer = NULL;
              sock->send_length = 0;
              epoll_ctl(manager->epoll, EPOLL_CTL_MOD, sock->sfd, &((struct epoll_event) {
                .events = EPOLLIN | EPOLLRDHUP,
                .data = (epoll_data_t) {
                  .fd = sock->sfd
                }
              }));
              if(sock->onsent != NULL) {
                sock->onsent(sock);
              }
            } else {
              (void) memmove((char*) sock->send_buffer, (char*) sock->send_buffer + bytes, sock->send_length);
              sock->send_buffer = realloc(sock->send_buffer, sock->send_length);
            }
          }
        }
      } else {
        struct NETServer* serv = (struct NETServer*) sock;
        if((events[i].events & EPOLLERR) != 0) {
          puts("EPOLLERR");
          serv->onerror(serv);
        }
        if((events[i].events & EPOLLIN) != 0) {
          puts("new connection pending");
        }
      }
    }
    (void) pthread_sigmask(SIG_UNBLOCK, &mask, NULL);
    (void) sigpending(&mask);
    if(sigismember(&mask, SIGRTMAX - 1)) {
      (void) close(manager->epoll);
      net_avl_multithread_free(&manager->tree);
    }
    (void) sigemptyset(&mask);
    (void) sigaddset(&mask, SIGRTMAX - 1);
  }
  return NULL;
}

#undef MSG_BUFFER_LEN
#undef manager

int InitConnManager(struct NETConnManager* const manager, const uint32_t avg_size) {
  const int epoll = epoll_create1(0);
  if(epoll == -1) {
    return errno;
  }
  manager->tree = net_avl_multithread_tree(avg_size);
  int err = net_avl_multithread_init(&manager->tree);
  if(err != 0) {
    (void) close(epoll);
    return err;
  }
  err = pthread_create(&manager->thread, NULL, EPollThread, manager);
  if(err != 0) {
    (void) close(epoll);
    net_avl_multithread_free(&manager->tree);
    return err;
  }
  manager->epoll = epoll;
  return 0;
}

int InitEventlessConnManager(struct NETConnManager* const manager, const uint32_t avg_size) {
  manager->tree = net_avl_multithread_tree(avg_size);
  return net_avl_multithread_init(&manager->tree);
}

static int NETConnManagerAdd(struct NETConnManager* const manager, const struct NETSocket socket, const unsigned int flags) {
  int err = net_avl_multithread_insert(&manager->tree, socket);
  if(err != 0) {
    return err;
  }
  err = epoll_ctl(manager->epoll, EPOLL_CTL_ADD, socket.sfd, &((struct epoll_event) {
    .events = EPOLLIN | EPOLLRDHUP | flags,
    .data = (epoll_data_t) {
      .fd = socket.sfd
    }
  }));
  if(err != 0) {
    net_avl_multithread_delete(&manager->tree, socket.sfd);
    err = errno;
  }
  return err;
}

int AddSocket(struct NETConnManager* const manager, const struct NETSocket socket) {
  if(socket.send_length != 0) {
    return NETConnManagerAdd(manager, socket, EPOLLOUT);
  } else {
    return NETConnManagerAdd(manager, socket, 0);
  }
}

int AddServer(struct NETConnManager* const manager, const struct NETServer server) {
  return NETConnManagerAdd(manager, *((struct NETSocket*)&server), EPOLLET);
}

struct NETSocket* GetSocket(struct NETConnManager* const manager, const int sfd) {
  return net_avl_multithread_search(&manager->tree, sfd);
}

struct NETServer* GetServer(struct NETConnManager* const manager, const int sfd) {
  return (struct NETServer*) GetSocket(manager, sfd);
}

void DeleteEventlessSocket(struct NETConnManager* const manager, const int sfd) {
  net_avl_multithread_delete(&manager->tree, sfd);
}

void DeleteEventlessServer(struct NETConnManager* const manager, const int sfd) {
  DeleteEventlessSocket(manager, sfd);
}

int DeleteSocket(struct NETConnManager* const manager, const int sfd) {
  DeleteEventlessSocket(manager, sfd);
  int err = epoll_ctl(manager->epoll, EPOLL_CTL_DEL, sfd, (struct epoll_event*) manager);
  if(err != 0) {
    err = errno;
  }
  return err;
}

int DeleteServer(struct NETConnManager* const manager, const int sfd) {
  return DeleteSocket(manager, sfd);
}

void FreeConnManager(struct NETConnManager* const manager) {
  (void) pthread_cancel(manager->thread);
  (void) pthread_sigqueue(manager->thread, SIGRTMAX - 1, (union sigval) { .sival_ptr = NULL });
}

#define pool ((struct NETAcceptThreadPool*) info->si_value.sival_ptr)

static void AcceptThreadHandler(int sig, siginfo_t* info, void* ucontext) {
  puts("sig");
  if(pool != NULL) {
    puts("wow pool null");
    uint32_t g = atomic_fetch_sub(&pool->amount, 1);
    printf("signal handler amount %u\n", g);
    if(g == 1) {
      puts("end");
      (void) pthread_mutex_destroy(&pool->mutex);
      free(pool->threads);
      puts("end 1");
      if(pool->onstop != NULL) {
        pool->onstop(pool);
      }
    }
    pthread_exit(NULL);
  }
}

#undef pool

static void AcceptThreadTerminationCheck(struct NETAcceptThreadPool* const pool) {
  sigset_t mask;
  (void) sigfillset(&mask);
  (void) sigdelset(&mask, SIGRTMAX - 2);
  (void) pthread_sigmask(SIG_SETMASK, &mask, NULL);
  (void) sigpending(&mask);
  if(sigismember(&mask, SIGRTMAX - 2)) {
    if(atomic_fetch_sub(&pool->amount, 1) == 1) {
      (void) pthread_mutex_destroy(&pool->mutex);
      free(pool->threads);
      if(pool->onstop != NULL) {
        pool->onstop(pool);
      }
    }
    pthread_exit(NULL);
  }
}

#define pool ((struct NETAcceptThreadPool*) a)

static void* AcceptThread(void* a) {
  sigset_t mask;
  (void) sigaction(SIGRTMAX - 2, &((struct sigaction) {
    .sa_flags = SA_SIGINFO,
    .sa_sigaction = AcceptThreadHandler
  }), NULL);
  (void) sigfillset(&mask);
  (void) sigdelset(&mask, SIGRTMAX - 2);
  (void) sigsuspend(&mask);
  (void) pthread_sigmask(SIG_SETMASK, &mask, NULL);
  (void) sigemptyset(&mask);
  (void) sigaddset(&mask, SIGRTMAX - 2);
  if(atomic_fetch_add(&pool->amount2, 1) == atomic_load(&pool->amount) && pool->onstart != NULL) {
    pool->onstart(pool);
  }
  struct sockaddr addr;
  socklen_t addr_len;
  while(1) {
    addr_len = sizeof(struct sockaddr);
    accept:;
    puts("accepting");
    int sfd = accept(pool->server->sfd, &addr, &addr_len);
    (void) pthread_sigmask(SIG_BLOCK, &mask, NULL);
    (void) pthread_mutex_lock(&pool->mutex);
    if(sfd == -1) {
      if(errno != ECONNABORTED) {
        pool->onerror(pool, sfd);
      }
      (void) pthread_mutex_unlock(&pool->mutex);
      AcceptThreadTerminationCheck(pool);
      goto accept;
    }
    if(pool->server->conn_count == pool->server->max_conn_count) {
      realloc:;
      int* const ptr = realloc(pool->server->connections, sizeof(int) * (pool->server->max_conn_count + pool->growth));
      if(ptr == NULL) {
        errno = ENOMEM;
        pool->onerror(pool, sfd);
        AcceptThreadTerminationCheck(pool);
        goto realloc;
      }
      pool->server->connections = ptr;
      pool->server->max_conn_count += pool->growth;
    }
    pool->server->connections[pool->server->conn_count++] = sfd;
    if(pool->server->onconnection != NULL) {
      pool->server->onconnection(pool->server, (struct NETSocket) {
        .addr = addr,
        .onmessage = NULL,
        .onclose = NULL,
        .onerror = NULL,
        .onsent = NULL,
        .send_buffer = NULL,
        .send_length = 0,
        .addrlen = addr_len,
        .state = NET_OPEN,
        .flags = pool->server->flags ^ AI_PASSIVE,
        .server = pool->server->sfd,
        .family = pool->server->family,
        .socktype = pool->server->socktype,
        .protocol = pool->server->protocol,
        .sfd = sfd
      });
    }
    (void) pthread_mutex_unlock(&pool->mutex);
    AcceptThreadTerminationCheck(pool);
  }
  return NULL;
}

#undef pool

static void ReadyAcceptThreadPool(struct NETAcceptThreadPool* const pool) {
  const uint32_t amount = atomic_load(&pool->amount);
  for(uint32_t i = 0; i < amount; ++i) {
    (void) pthread_sigqueue(pool->threads[i], SIGRTMAX - 2, (union sigval) { .sival_ptr = NULL });
  }
}

int InitAcceptThreadPool(struct NETAcceptThreadPool* const pool, const uint32_t amount, const uint32_t growth) {
  int err = pthread_mutex_init(&pool->mutex, NULL);
  if(err != 0) {
    return err;
  }
  pthread_attr_t attr;
  if(pthread_attr_init(&attr) != 0) {
    (void) pthread_mutex_destroy(&pool->mutex);
    return ENOMEM;
  }
  (void) pthread_attr_setstacksize(&attr, 525312);
  pool->threads = malloc(sizeof(pthread_t) * amount);
  if(pool->threads == NULL) {
    (void) pthread_mutex_destroy(&pool->mutex);
    (void) pthread_attr_destroy(&attr);
    return ENOMEM;
  }
  pool->growth = growth;
  sigset_t mask;
  (void) sigfillset(&mask);
  sigset_t oldmask;
  (void) pthread_sigmask(SIG_SETMASK, &mask, &oldmask);
  for(uint32_t i = 0; i < amount; ++i) {
    err = pthread_create(&pool->threads[i], &attr, AcceptThread, pool);
    if(err != 0) {
      (void) pthread_sigmask(SIG_SETMASK, &oldmask, NULL);
      (void) pthread_attr_destroy(&attr);
      atomic_store(&pool->amount, i + 1);
      FreeAcceptThreadPool(pool);
      return err;
    }
  }
  (void) pthread_sigmask(SIG_SETMASK, &oldmask, NULL);
  (void) pthread_attr_destroy(&attr);
  atomic_store(&pool->amount, amount);
  atomic_store(&pool->amount2, 1);
  atomic_store(&pool->counterrorist, 0);
  ReadyAcceptThreadPool(pool);
  return 0;
}

void FreeAcceptThreadPool(struct NETAcceptThreadPool* const pool) {
  //puts("freeacceptthreadpool");
  const uint32_t amount = atomic_load(&pool->amount);
  for(uint32_t i = 0; i < amount; ++i) {
    (void) pthread_sigqueue(pool->threads[i], SIGRTMAX - 2, (union sigval) { .sival_ptr = pool });
  }
}