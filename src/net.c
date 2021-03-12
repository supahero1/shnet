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

#define arr ((struct AsyncGAIArray*) a)

__nonnull((1))
static void* AsyncGAIThread(void* a) {
  struct addrinfo* res;
  int i = 0;
  for(; i < arr->count; ++i) {
    arr->infos[i].callback(res, GetAddrInfo(arr->infos[i].hostname, arr->infos[i].service, arr->infos[i].flags, &res));
    freeaddrinfo(res);
  }
  return NULL;
}

#undef arr
#define arr ((struct AsyncNETArray*) a)

__nonnull((1))
static void* AsyncTCPConnectThread(void* a) {
  struct addrinfo* res;
  struct addrinfo* n;
  struct NETSocket nets;
  const enum NETFlags type = arr->type;
  int err;
  int sfd;
  int i = 0;
  /*
  >= 0 = success
  -1 = getaddrinfo err
  -2 = connect err
  -3 = no socket succeeded
  */
  for(; i < arr->count; ++i) {
    if(type == NET_GAI) {
      err = GetAddrInfo(arr->infos.gai[i].hostname, arr->infos.gai[i].service, arr->infos.gai[i].flags, &res);
      if(err != 0) {
        errno = err;
        arr->infos.gai[i].callback(NULL, -1);
        continue;
      }
    } else {
      res = arr->infos.nogai[i].addrinfos;
    }
    for(n = res; n != NULL; n = n->ai_next) {
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
        if(type == NET_GAI) {
          arr->infos.gai[i].callback(&nets, -2);
          if(n->ai_next == NULL) {
            arr->infos.gai[i].callback(&nets, -3);
            break;
          }
        } else {
          arr->infos.nogai[i].callback(&nets, -2);
          if(n->ai_next == NULL) {
            arr->infos.nogai[i].callback(&nets, -3);
            break;
          }
        }
        continue;
      } else {
        err = fcntl(sfd, F_GETFL, 0);
        if(err == -1) {
          err = 0;
        }
        err = fcntl(sfd, F_SETFL, err | O_NONBLOCK);
        if(err == -1) {
          printf("err at first fcntl: %s\n", strerror(errno));
        }
        nets.state = NET_OPEN;
        if(type == NET_GAI) {
          arr->infos.gai[i].callback(&nets, sfd);
        } else {
          arr->infos.nogai[i].callback(&nets, sfd);
        }
        break;
      }
    }
    freeaddrinfo(res);
  }
  return NULL;
}

__nonnull((1))
static void* AsyncTCPListenThread(void* a) {
  struct addrinfo* res;
  struct addrinfo* n;
  struct NETSocket nets;
  const enum NETFlags type = arr->type;
  int err;
  int sfd;
  int i = 0;
  int yes = 1;
  /*
  >= 0 = success
  -1 = getaddrinfo err
  -2 = bind err
  -3 = listen err
  -3 = no socket succeeded
  */
  for(; i < arr->count; ++i) {
    if(type == NET_GAI) {
      err = GetAddrInfo(arr->infos.gai[i].hostname, arr->infos.gai[i].service, arr->infos.gai[i].flags | AI_PASSIVE, &res);
      if(err != 0) {
        errno = err;
        arr->infos.gai[i].callback(NULL, -1);
        continue;
      }
    } else {
      res = arr->infos.nogai[i].addrinfos;
    }
    for(n = res; n != NULL; n = n->ai_next) {
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
      (void) setsockopt(sfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));
      err = bind(sfd, n->ai_addr, n->ai_addrlen);
      if(err != 0) {
        (void) close(sfd);
        if(type == NET_GAI) {
          arr->infos.gai[i].callback(&nets, -2);
        } else {
          arr->infos.nogai[i].callback(&nets, -2);
        }
        continue;
      }
      puts("bind succeeded");
      err = listen(sfd, 128);
      if(err != 0) {
        (void) close(sfd);
        if(type == NET_GAI) {
          arr->infos.gai[i].callback(&nets, -3);
          if(n->ai_next == NULL) {
            arr->infos.gai[i].callback(&nets, -4);
            break;
          }
        } else {
          arr->infos.nogai[i].callback(&nets, -3);
          if(n->ai_next == NULL) {
            arr->infos.nogai[i].callback(&nets, -4);
            break;
          }
        }
        continue;
      } else {
        err = fcntl(sfd, F_GETFL, 0);
        if(err == -1) {
          err = 0;
        }
        err = fcntl(sfd, F_SETFL, err | O_NONBLOCK);
        if(err == -1) {
          printf("err at first fcntl: %s\n", strerror(errno));
        }
        nets.state = NET_OPEN;
        if(type == NET_GAI) {
          arr->infos.gai[i].callback(&nets, sfd);
        } else {
          arr->infos.nogai[i].callback(&nets, sfd);
        }
        break;
      }
    }
    freeaddrinfo(res);
  }
  return NULL;
}

#undef arr

static void dummy_signal_handler(__unused int sig) {}

#define manager ((struct NETConnectionManager*) s)

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
  while(1) {
    puts("epoll is waiting");
    err = epoll_wait(manager->epoll, events, 100, -1);
    if(err == -1) {
      printf("epoll got -1 code, errno: %d | %s\n", errno, strerror(errno));
      close(manager->epoll);
      net_avl_free(&manager->avl_tree);
      return NULL;
    }
    for(i = 0; i < err; ++i) {
      net_avl_search(&manager->avl_tree, events[i].data.fd, events[i].events);
    }
  }
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
  net_avl_tree(&manager->avl_tree, avg_size);
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
  (void) pthread_sigqueue(manager->thread, SIGRTMAX, (union sigval) { .sival_ptr = NULL });
}

int AsyncGetAddrInfo(struct AsyncGAIArray* const array) {
  pthread_t t;
  return pthread_create(&t, NULL, AsyncGAIThread, array);
}

int AsyncTCPConnect(struct AsyncNETArray* const array) {
  pthread_t t;
  return pthread_create(&t, NULL, AsyncTCPConnectThread, array);
}

int AsyncTCPListen(struct AsyncNETArray* const array) {
  pthread_t t;
  return pthread_create(&t, NULL, AsyncTCPListenThread, array);
}
