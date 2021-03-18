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

void TCPNoBlock(const int sfd) {
  int err = fcntl(sfd, F_GETFL, 0);
  if(err == -1) {
    err = 0;
  }
  (void) fcntl(sfd, F_SETFL, err | O_NONBLOCK);
}

void TCPCorkOn(const int sfd) {
  (void) setsockopt(sfd, SOL_TCP, TCP_CORK, &(int){1}, sizeof(int));
}

void TCPCorkOff(const int sfd) {
  (void) setsockopt(sfd, SOL_TCP, TCP_CORK, &(int){0}, sizeof(int));
}

void TCPNoDelayOn(const int sfd) {
  (void) setsockopt(sfd, IPPROTO_TCP, TCP_NODELAY, &(int){1}, sizeof(int));
}

void TCPNoDelayOff(const int sfd) {
  (void) setsockopt(sfd, IPPROTO_TCP, TCP_NODELAY, &(int){0}, sizeof(int));
}

void TCPSocketFree(struct NETSocket* const socket) {
  if(socket->send_buffer != NULL) {
    free(socket->send_buffer);
    socket->send_buffer = NULL;
    socket->send_length = 0;
  }
  close(socket->sfd);
}

void TCPServerFree(struct NETServer* const server) {
  size_t i;
  if(server->connections != NULL) {
    for(i = 0; i < server->conn_count; ++i) {
      TCPShutdown(GetSocket(server->manager, server->connections[i]));
    }
    free(server->connections);
    server->connections = NULL;
    server->conn_count = 0;
  }
  close(server->sfd);
}

__nonnull((1))
static void TCPKill(struct NETSocket* const socket) {
  socket->state = NET_CLOSED;
  if(socket->onclose != NULL) {
    socket->onclose(socket);
  }
  TCPSocketFree(socket);
}

int TCPSend(struct NETSocket* const socket, void* const buffer, const size_t length, struct NETConnManager* const manager) {
  void* ptr;
  long bytes = send(socket->sfd, buffer, length, MSG_NOSIGNAL);
  puts("TCPSend()");
  if(bytes == -1) {
    if(errno == EAGAIN) {
      puts("not everything sent");
      ptr = realloc(socket->send_buffer, socket->send_length + length - bytes);
      if(ptr == NULL) {
        return ENOMEM;
      }
      socket->send_buffer = ptr;
      (void) memcpy(socket->send_buffer + socket->send_length, buffer + bytes, length - bytes);
      socket->send_length += length - bytes;
      if(manager != NULL) {
        epoll_ctl(manager->epoll, EPOLL_CTL_MOD, socket->sfd, &((struct epoll_event) {
          .events = EPOLLIN | EPOLLRDHUP,
          .data = (epoll_data_t) {
            .fd = socket->sfd
          }
        }));
      }
      return 0;
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
    return 0;
  } else {
    puts("sent lower amount than requested but no error");
    exit(1);
  }
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

#define MSG_BUFFER_LEN 1024

__nonnull((1))
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

static void dummy_signal_handler(__unused int sig) {}

#define manager ((struct NETConnManager*) s)

__nothrow __nonnull((1))
static void EPollThreadCleanup(void* s) {
  close(manager->epoll);
  net_avl_free(&manager->avl_tree);
  (void) pthread_mutex_destroy(&manager->mutex);
}

__nonnull((1))
static void* EPollThread(void* s) {
  sigset_t mask;
  struct epoll_event events[100];
  struct NETSocket* sock;
  struct NETServer* serv;
  struct sockaddr addr;
  socklen_t addr_len;
  int err;
  int i;
  int temp;
  long bytes;
  uint8_t mem[MSG_BUFFER_LEN];
  int* ptr;
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
    (void) pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);
    if(err == -1) {
      printf("epoll_wait err: %s\n", strerror(errno));
      close(manager->epoll);
      net_avl_free(&manager->avl_tree);
      (void) pthread_mutex_destroy(&manager->mutex);
      return NULL;
    }
    for(i = 0; i < err; ++i) {
      (void) pthread_mutex_lock(&manager->mutex);
      sock = net_avl_search(&manager->avl_tree, events[i].data.fd);
      (void) pthread_mutex_unlock(&manager->mutex);
      if((sock->flags & AI_PASSIVE) == 0) {
        if((events[i].events & EPOLLHUP) != 0) { // they can message us but we can't message them
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
        if((events[i].events & EPOLLRDHUP) != 0) { // they can't message us but we can message them
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
        if((events[i].events & EPOLLERR) != 0) { // an event
          getsockopt(sock->sfd, SOL_SOCKET, SO_ERROR, &temp, NULL);
          printf("got an event, code %d, message: %s\n", temp, strerror(temp));
        }
        if((events[i].events & EPOLLIN) != 0) { // can receive
          (void) EPollSocketGetMessage(manager, sock, mem, 1);
        }
        if(sock->send_length != 0 && sock->state != NET_SEND_CLOSING && (events[i].events & EPOLLOUT) != 0) { // can send
          puts("sending what we have in buffer");
          bytes = send(sock->sfd, sock->send_buffer, sock->send_length, MSG_NOSIGNAL);
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
              (void) memmove(sock->send_buffer, sock->send_buffer + bytes, sock->send_length);
              sock->send_buffer = realloc(sock->send_buffer, sock->send_length);
            }
          }
        }
      } else {
        serv = (struct NETServer*) sock;
        if((events[i].events & EPOLLERR) != 0) {
          puts("EPOLLERR");
          serv->onerror(serv);
        }
        if((events[i].events & EPOLLIN) != 0) {
          puts("new connection pending");
          ptr = realloc(serv->connections, sizeof(int) * (serv->conn_count + 1));
          if(ptr == NULL) {
            errno = ENOMEM;
            serv->onerror(serv);
            continue;
          }
          serv->connections = ptr;
          temp = accept(serv->sfd, &addr, &addr_len);
          if(temp == -1) {
            if(errno == ECONNABORTED) {
              puts("nvm aborted lol");
              continue;
            } else {
              serv->onerror(serv);
              continue;
            }
          }
          serv->connections[serv->conn_count++] = temp;
          if(serv->onconnection != NULL) {
            serv->onconnection(serv, (struct NETSocket) {
              .addr = addr,
              .onmessage = NULL,
              .onclose = NULL,
              .onerror = NULL,
              .onsent = NULL,
              .send_buffer = NULL,
              .send_length = 0,
              .addrlen = addr_len,
              .state = NET_OPEN,
              .flags = serv->flags ^ AI_PASSIVE,
              .server = serv->sfd,
              .socktype = serv->socktype,
              .protocol = serv->protocol,
              .sfd = temp
            });
          }
        }
      }
    }
    (void) pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
    pthread_testcancel();
  }
  pthread_cleanup_pop(1);
  return NULL;
}

#undef MSG_BUFFER_LEN
#undef manager

int InitConnManager(struct NETConnManager* const manager, const uint32_t avg_size) {
  const int epoll = epoll_create1(0);
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
    return err;
  }
  manager->epoll = epoll;
  return pthread_mutex_init(&manager->mutex, NULL);
}

int InitEventlessConnManager(struct NETConnManager* const manager, const uint32_t avg_size) {
  manager->avl_tree = net_avl_tree(avg_size);
  const int err = net_avl_init(&manager->avl_tree);
  if(err != 0) {
    return err;
  }
  return pthread_mutex_init(&manager->mutex, NULL);
}

__nonnull((1))
static int NETConnManagerAdd(struct NETConnManager* const manager, const struct NETSocket socket, const int flags) {
  (void) pthread_mutex_lock(&manager->mutex);
  int err = net_avl_insert(&manager->avl_tree, socket);
  (void) pthread_mutex_unlock(&manager->mutex);
  if(err != 0) {
    puts("failed to add a socket");
    return err;
  }
  if(socket.send_length != 0) {
    err = EPOLLOUT;
  }
  err = epoll_ctl(manager->epoll, EPOLL_CTL_ADD, socket.sfd, &((struct epoll_event) {
    .events = EPOLLIN | EPOLLRDHUP | err | flags,
    .data = (epoll_data_t) {
      .fd = socket.sfd
    }
  }));
  if(err != 0) {
    (void) pthread_mutex_lock(&manager->mutex);
    net_avl_delete(&manager->avl_tree, socket.sfd);
    (void) pthread_mutex_unlock(&manager->mutex);
    return errno;
  }
  return 0;
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
  (void) pthread_mutex_lock(&manager->mutex);
  struct NETSocket* const s = net_avl_search(&manager->avl_tree, sfd);
  (void) pthread_mutex_unlock(&manager->mutex);
  return s;
}

struct NETServer* GetServer(struct NETConnManager* const manager, const int sfd) {
  return (struct NETServer*) GetSocket(manager, sfd);
}

int DeleteSocket(struct NETConnManager* const manager, const int sfd) {
  (void) pthread_mutex_lock(&manager->mutex);
  net_avl_delete(&manager->avl_tree, sfd);
  (void) pthread_mutex_unlock(&manager->mutex);
  int err = epoll_ctl(manager->epoll, EPOLL_CTL_DEL, sfd, (struct epoll_event*) manager);
  if(err != 0) {
    err = errno;
  }
  return err;
}

int DeleteServer(struct NETConnManager* const manager, const int sfd) {
  return DeleteSocket(manager, sfd);
}

void DeleteEventlessSocket(struct NETConnManager* const manager, const int sfd) {
  (void) pthread_mutex_lock(&manager->mutex);
  net_avl_delete(&manager->avl_tree, sfd);
  (void) pthread_mutex_unlock(&manager->mutex);
}

void DeleteEventlessServer(struct NETConnManager* const manager, const int sfd) {
  return DeleteEventlessSocket(manager, sfd);
}

void FreeConnManager(struct NETConnManager* const manager) {
  (void) pthread_cancel(manager->thread);
  (void) pthread_sigqueue(manager->thread, SIGRTMAX, (union sigval) { .sival_ptr = NULL });
}

int SyncTCPConnect(struct addrinfo* const res, struct NETSocket* const sock) {
  struct addrinfo* n;
  int sfd;
  int err;
  sock->send_buffer = NULL;
  sock->send_length = 0;
  for(n = res; n != NULL; n = n->ai_next) {
    sock->addr = *n->ai_addr;
    sock->addrlen = n->ai_addrlen;
    sock->flags = n->ai_flags;
    sock->family = n->ai_family;
    sock->socktype = n->ai_socktype;
    sock->protocol = n->ai_protocol;
    sock->sfd = -1;
    sfd = socket(n->ai_family, n->ai_socktype, n->ai_protocol);
    if(sfd == -1) {
      printf("creating a socket failed with reason: %s\n", strerror(errno));
      continue;
    }
    printf("addrlen: %d\n", sock->addrlen);
    sock->sfd = sfd;
    printf("we connecting at   %ld\n", GetTime(0));
    printf("ip: %s\n", inet_ntoa(((struct sockaddr_in*) n->ai_addr)->sin_addr));
    err = connect(sfd, n->ai_addr, n->ai_addrlen);
    printf("done connecting at %ld\n", GetTime(0));
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
  struct addrinfo* n;
  int sfd;
  int err;
  serv->manager = NULL;
  serv->connections = NULL;
  serv->conn_count = 0;
  for(n = res; n != NULL; n = n->ai_next) {
    serv->addr = *n->ai_addr;
    serv->addrlen = n->ai_addrlen;
    serv->flags = n->ai_flags;
    serv->family = n->ai_family;
    serv->socktype = n->ai_socktype;
    serv->protocol = n->ai_protocol;
    serv->sfd = -1;
    sfd = socket(n->ai_family, n->ai_socktype, n->ai_protocol);
    if(sfd == -1) {
      printf("creating a socket failed with reason: %s\n", strerror(errno));
      continue;
    }
    serv->sfd = sfd;
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

__nonnull((1))
static void* AsyncTCPConnectThread(void* a) {
  struct addrinfo* n;
  struct NETSocket sock;
  int err;
  int sfd;
  /*
  >0 = success
  -1 = connect err
  -2 = no socket succeeded
  */
  sock.onmessage = NULL;
  sock.onclose = NULL;
  sock.onerror = NULL;
  sock.onsent = NULL;
  sock.send_buffer = 0;
  sock.send_length = 0;
  for(n = arr->addrinfo; n != NULL; n = n->ai_next) {
    sock.addr = *n->ai_addr;
    sock.addrlen = n->ai_addrlen;
    sock.flags = n->ai_flags;
    sock.family = n->ai_family;
    sock.socktype = n->ai_socktype;
    sock.protocol = n->ai_protocol;
    sock.sfd = -1;
    sfd = socket(n->ai_family, n->ai_socktype, n->ai_protocol);
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
    printf("we connecting at %ld\n", GetTime(0));
    err = connect(sfd, n->ai_addr, n->ai_addrlen);
    printf("done connecting at %ld\n", GetTime(0));
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

__nonnull((1))
static void* AsyncTCPListenThread(void* a) {
  struct addrinfo* n;
  struct NETServer serv;
  int err;
  int sfd;
  /*
  >0 = success
  -1 = bind err
  -2 = listen err
  -3 = no socket succeeded
  */
  serv.onconnection = NULL;
  serv.onerror = NULL;
  serv.manager = NULL;
  serv.connections = NULL;
  serv.conn_count = 0;
  for(n = arr->addrinfo; n != NULL; n = n->ai_next) {
    serv.addr = *n->ai_addr;
    serv.addrlen = n->ai_addrlen;
    serv.flags = n->ai_flags;
    serv.family = n->ai_family;
    serv.socktype = n->ai_socktype;
    serv.protocol = n->ai_protocol;
    serv.sfd = -1;
    sfd = socket(n->ai_family, n->ai_socktype, n->ai_protocol);
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
    err = bind(sfd, n->ai_addr, n->ai_addrlen);
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