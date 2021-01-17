#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/syscall.h>

#include "net.h"

#include <stdio.h>
#include <string.h>
#include <arpa/inet.h>

/*int ParseHTTP_URL(const char* const u, struct HTTP_URL* const restrict url, const enum HTTP_URL_FLAGS flags) {
  unsigned int i = 0;
  unsigned int j = 0;
  const char* a = u;
  const char* b;
  const char* c;
  const char* d;
  const size_t len = strlen(u);
  // these flags are so useless
  const int PPR = flags & NET_PPR; // parse protocol
  const int PHO = flags & NET_PHO; // parse host
  const int POR = flags & NET_POR; // parse origin
  const int PPO = flags & NET_PPO; // parse port
  const int PPA = flags & NET_PPA; // parse pathname
  const int PQU = flags & NET_PQU; // parse query
  const int PHA = flags & NET_PHA; // parse hash
  
  if(PPR != 0) {
    if(u[4] == 's') {
      url->protocol = "https";
      url->protocol_len = 5;
    } else {
      url->protocol = "http";
      url->protocol_len = 4;
    }
  }
  if(PPO != 0) {
    if(u[7] == '[') {
      b = strchr(strchr(u + 11, ']'), ':');
    } else if(u[8] == '[') {
      b = strchr(strchr(u + 12, ']'), ':');
    } else {
      b = strchr(u + 10, ':');
    }
    if(b == NULL) {
      url->port = "80";
      url->port_len = 2;
    } else if(b[1] == '/' || b[1] == '\0') {
      url->port = "80";
      url->port_len = 2;
    } else {
      url->port = b + 1;
      if(b[2] == '/' || b[2] == '\0') {
        url->port_len = 1;
      } else if(b[3] == '/' || b[3] == '\0') {
        url->port_len = 2;
      } else if(b[4] == '/' || b[4] == '\0') {
        url->port_len = 3;
      } else if(b[5] == '/' || b[5] == '\0') {
        url->port_len = 4;
      } else {
        url->port_len = 5;
      }
    }
  }
  if(PHO != 0) {
    if(u[4] == 's') {
      url->host = u + 8;
    } else {
      url->host = u + 7;
    }
    if(b != NULL) {
      url->host_len = (uintptr_t) b - (uintptr_t) a;
    } else {
      c = strchr(a + 1, "/");
      if(c != NULL) {
        url->host_len = (uintptr_t) c - (uintptr_t) a;
      } else {
        url->host_len = (uintptr_t) url + len - (uintptr_t) a;
      }
    }
  }
  if(POR != 0) {
    
  }
}*/

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
  /*(void) sigaction(SIGIO, &((struct sigaction) {
    .sa_flags = 0,
    .sa_handler = log // add log function see what happens
  }), NULL);*/
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
        err = fcntl(sfd, F_SETFL, fcntl(sfd, F_GETFL, 0) | O_NONBLOCK);
        if(err == -1) {
          printf("err at first fcntl: %s\n", strerror(errno));
        }
        err = fcntl(sfd, F_SETOWN_EX, &((struct f_owner_ex) {
          .type = F_OWNER_TID,
          .pid = syscall(SYS_gettid)
        }));
        if(err == -1) {
          printf("err at second fcntl: %s\n", strerror(errno));
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
  /*(void) sigaction(SIGIO, &((struct sigaction) {
    .sa_flags = 0,
    .sa_handler = log // add log function see what happens
  }), NULL);*/
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
        err = fcntl(sfd, F_SETFL, fcntl(sfd, F_GETFL, 0) | O_NONBLOCK);
        if(err == -1) {
          printf("err at first fcntl: %s\n", strerror(errno));
        }
        err = fcntl(sfd, F_SETOWN_EX, &((struct f_owner_ex) {
          .type = F_OWNER_TID,
          .pid = syscall(SYS_gettid)
        }));
        if(err == -1) {
          printf("err at second fcntl: %s\n", strerror(errno));
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

// old code below, really old, before work distributor
/*int get_sent_buffered_amount(int sockfd) {
  int value;
  (void) ioctl(sockfd, SIOCOUTQ, &value);
  return value;
}
int get_received_buffered_amount(int sockfd) {
  int value;
  (void) ioctl(sockfd, SIOCINQ, &value);
  return value;
}

int set_cork(int sockfd, const int val) {
  return setsockopt(sockfd, IPPROTO_TCP, TCP_CORK, &val, sizeof(int));
}

static int httpsocket(struct addrinfo* const res, pid_t pid) {
  struct addrinfo* cpy;
  int sfd;
  int n = 1;
  for(cpy = res; cpy != NULL; cpy = cpy->ai_next) {
    sfd = socket(cpy->ai_family, SOCK_SEQPACKET | SOCK_NONBLOCK, 0);
    if(sfd == -1) {
      puts("Couldnt establish socket");
      continue;
    }
    (void) setsockopt(sfd, IPPROTO_TCP, TCP_NODELAY, &n, sizeof(int));
    (void) ioctl(sfd, FIOASYNC, &n);
    (void) ioctl(sfd, SIOCSPGRP, &pid);
    (void) fcntl(sfd, F_SETSIG, &pid);
    puts("Got a socket");
    
    if(connect(sfd, cpy->ai_addr, cpy->ai_addrlen) == 0) {
      puts("Got a connection!");
      printf("Our connection has %d mem cached\n", get_sent_buffered_amount(sfd));
      break;
    } else {
      puts("no connection with errno:");
      printerrno();
    }
    close(sfd);
  }
  freeaddrinfo(res);
  return sfd;
}
static int httpserver(struct addrinfo* res, const int flags) {
  struct addrinfo* cpy;
  int sfd;
  int yes = 1;
  for(cpy = res; cpy != NULL; cpy = cpy->ai_next) {
    sfd = socket(cpy->ai_family, SOCK_STREAM | SOCK_NONBLOCK, 0);
    if(sfd == -1) {
      continue;
    }
    (void) setsockopt(sfd, SOL_SOCKET, SO_KEEPALIVE, &yes);
    (void) setsockopt(sfd, IPPROTO_TCP, TCP_NODELAY, &yes, sizeof(int));
    (void) ioctl(sfd, FIOASYNC, &n);
    (void) ioctl(sfd, SIOCSPGRP, &pid);
    if(bind(sfd, cpy->ai_addr, cpy->ai_addrlen) == 0) {
      break;
    }
    close(sfd);
  }
  freeaddrinfo(res);
  if(cpy == NULL) {
    return -1;
  }
  return sfd;
}

int IPConnect(const char hostname[], const char port[], const int family, pid_t pid) {
  struct addrinfo* res;
  int err = gai(res, hostname, AI_NUMERICHOST, port, family, 0);
  if(err != 0) {
    puts("failed to get info");
    return err;
  }
  puts("got info");
  return httpsocket(res, pid);
}
int HostConnect(const char hostname[], const char port[], const int family, pid_t pid) {
  struct addrinfo* res;
  int err = gai(res, hostname, 0, port, family, 0);
  if(err != 0) {
    puts("failed to get info");
    return err;
  }
  puts("got info");
  return httpsocket(res, pid);
}

int ServerBind(const char port[], const int family, pid_t pid) {
  return httpserver(gai(NULL, 0, port, family, AI_PASSIVE), pid);
}*/
