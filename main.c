#include "check.h"
#include "def.h"

#include "timeout/timeout.h"
//#include "net/net_base.h"
//#include "net/net_avl.h"
//#include "net/net.h"

#include <math.h>
#include <sched.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/mman.h>
#include <stdio.h>
#include <stdatomic.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <pthread.h>
#include <errno.h>
#include <unistd.h>
#include <linux/sockios.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <netdb.h>
#include <fcntl.h>
#include <time.h>
#include <stdbool.h>
#include <semaphore.h>
#include <sys/epoll.h>
#include <limits.h>

#define amount 1000

void timeout_callback(void* data) {
  uint32_t num = atomic_fetch_add((_Atomic uint32_t*) data, 1);
  printf("callback nr %u bro", num);
}

int main() {
  _Atomic uint32_t incred = 1;
  struct Timeout timeout = Timeout();
  int err = StartTimeoutThread(&timeout, TIME_ALWAYS);
  if(err != 0) {
    puts("0");
    exit(1);
  }
  (void) getc(stdin);
  struct TimeoutObject work[amount];
  for(uint32_t i = 0; i < amount; ++i) {
    work[i] = (struct TimeoutObject) {
      .func = timeout_callback,
      .data = &incred,
      .time = GetTime(i * 1000000)
    };
  }
  (void) getc(stdin);
  err = AddTimeout(&timeout, work, amount);
  if(err != 0) {
    puts("1");
    exit(1);
  }
  (void) getc(stdin);
  return 0;
}

/*void onmessage(struct NETSocket* socket) {
  puts("onmessage()");
  char buffer[50000];
  memset(buffer, 0, 50000);
  long bytes = recv(socket->sfd, buffer, 50000, 0);
  printf("nice got %ld bytes\n", bytes);
  printf("%s\n", buffer);
}

void onclose(struct NETSocket* socket) {
  puts("onclose()");
}

void onerror(struct NETSocket* socket) {
  printf("got a socket error: %s\n", strerror(errno));
}

void onsent(struct NETSocket* socket) {
  puts("the socket sent all data we wanted it to send");
}

void onconnection(struct NETServer* server, struct NETSocket socket) {
  printf("new socket's sfd: %d\n", socket.sfd);
  socket.onmessage = onmessage;
  socket.onclose = onclose;
  socket.onerror = onerror;
  socket.onsent = onsent;
  int err = AddSocket(server->manager, socket);
  if(err != 0) {
    printf("error at AddSocket %d | %s\n", err, strerror(err));
    exit(1);
  }
  puts("AddSocket succeeded");
}

void serveronerror(struct NETServer* server) {
  printf("got a server error: %s\n", strerror(errno));
}

void asyncsocket(struct NETSocket* const socket, const int sfd) {
  switch(sfd) {
    case -1: {
      printf("connection error: %s\n", strerror(errno));
      break;
    }
    case -2: {
      puts("no socket succeeded");
      exit(1);
    }
    default: {
      int err;
      struct NETConnManager manager;
      err = InitConnManager(&manager, 5);
      if(err != 0) {
        printf("error at InitConnectionManager %d | %s\n", err, strerror(err));
        exit(1);
      }
      socket->onmessage = onmessage;
      socket->onclose = onclose;
      socket->onerror = onerror;
      socket->onsent = onsent;
      err = AddSocket(&manager, *socket);
      if(err != 0) {
        printf("error at AddSocket %d | %s\n", err, strerror(err));
        exit(1);
      }
      puts("AddSocket succeeded");
      char buf[] = "GET /index.html HTTP/1.1\r\nConnection: close\r\n\r\n";
      err = TCPSend(socket, buf, sizeof(buf), &manager);
      if(err != 0) {
        printf("error at TCPSend %d | %s\n", err, strerror(err));
        exit(1);
      }
      (void) getc(stdin);
      break;
    }
  }
}

void asyncgai(struct addrinfo* info, int status) {
  if(status == 0) {
    puts("gai succeeded");
    struct ANET_C* a = malloc(sizeof(struct ANET_C));
    if(a == NULL) {
      puts("cant malloc lol");
      exit(1);
    }
    struct addrinfo* copy = malloc(sizeof(struct addrinfo));
    if(copy == NULL) {
      puts("cant malloc lol");
      exit(1);
    }
    *copy = *info;
    a->handler = asyncsocket;
    a->addrinfo = copy;
    status = AsyncTCPConnect(a);
    if(status != 0) {
      printf("error at AsyncTCPConnect: %s\n", strerror(status));
      exit(1);
    }
  } else {
    printf("error at AsyncGetAddrInfo: %s\n", strerror(status));
    exit(1);
  }
}

void asyncserver(struct NETServer* const server, const int sfd) {
  switch(sfd) {
    case -1: {
      printf("connection error: %s\n", strerror(errno));
      break;
    }
    case -2: {
      puts("no socket succeeded");
      exit(1);
    }
    default: {
      int err;
      struct NETConnManager manager;
      err = InitConnManager(&manager, 5);
      if(err != 0) {
        printf("error at InitConnectionManager %d | %s\n", err, strerror(err));
        exit(1);
      }
      server->onconnection = onconnection;
      server->onerror = serveronerror;
      server->manager = &manager;
      err = AddServer(&manager, *server);
      if(err != 0) {
        printf("error at AddServer %d | %s\n", err, strerror(err));
        exit(1);
      }
      puts("AddServer succeeded");
      (void) getc(stdin);
      break;
    }
  }
}

void asyncgaiserver(struct addrinfo* info, int status) {
  if(status == 0) {
    puts("gai succeeded");
    struct ANET_L* a = malloc(sizeof(struct ANET_L));
    if(a == NULL) {
      puts("cant malloc lol");
      exit(1);
    }
    struct addrinfo* copy = malloc(sizeof(struct addrinfo));
    if(copy == NULL) {
      puts("cant malloc lol");
      exit(1);
    }
    *copy = *info;
    a->handler = asyncserver;
    a->addrinfo = copy;
    status = AsyncTCPListen(a);
    if(status != 0) {
      printf("error at AsyncTCPListen: %s\n", strerror(status));
      exit(1);
    }
  } else {
    printf("error at AsyncGetAddrInfo: %s\n", strerror(status));
    exit(1);
  }
}

int main(int argc, char** argv) {
  int err;
  printf("%ld\n", sizeof(pthread_mutex_t));
  if(argc < 5) {
    puts("Minimum amount of arguments is 4.");
    return 1;
  }
  if(argv[1][0] == 'a') {
    switch(argv[2][0]) {
      case 'c': {
        err = AsyncGetAddrInfo(&((struct ANET_GAIArray) {
          .arr = &((struct ANET_GAILookup) {
            .handler = asyncgai,
            .hostname = argv[3],
            .service = argv[4],
            .flags = IPv4
          }),
          .count = 1
        }));
        if(err != 0) {
          printf("error at AsyncGetAddrInfo: %s\n", strerror(err));
          exit(1);
        }
        (void) getc(stdin);
        break;
      }
      case 'l': {
        err = AsyncGetAddrInfo(&((struct ANET_GAIArray) {
          .arr = &((struct ANET_GAILookup) {
            .handler = asyncgaiserver,
            .hostname = argv[3],
            .service = argv[4],
            .flags = IPv4 | NET_SERVER
          }),
          .count = 1
        }));
        if(err != 0) {
          printf("error at AsyncGetAddrInfo: %s\n", strerror(err));
          exit(1);
        }
        (void) getc(stdin);
        break;
      }
      default: {
        puts("Option not recognized. The available options: l(isten) <host> <port>, c(onnect) <host> <port>");
        break;
      }
    }
  } else if(argv[1][0] == 's') {
    switch(argv[2][0]) {
      case 'c': {
        struct NETSocket sock;
        struct NETConnManager manager;
        err = InitConnManager(&manager, 5);
        if(err != 0) {
          printf("error at InitConnectionManager %d | %s\n", err, strerror(err));
          exit(1);
        }
        err = SyncTCP_GAIConnect(argv[3], argv[4], IPv4, &sock);
        if(err < -1) {
          printf("error at SyncTCP_GAIConnect %d | %s\n", err, strerror(err));
          exit(1);
        } else if(err == -1) {
          puts("no address succeeded at SyncTCP_GAIConnect");
          exit(1);
        }
        sock.onmessage = onmessage;
        sock.onclose = onclose;
        sock.onerror = onerror;
        sock.onsent = onsent;
        puts("SyncTCP_GAIConnect succeeded");
        err = AddSocket(&manager, sock);
        if(err != 0) {
          printf("error at AddSocket %d | %s\n", err, strerror(err));
          exit(1);
        }
        puts("AddSocket succeeded");
        char buf[] = "GET /index.html HTTP/1.1\r\nConnection: close\r\n\r\n";
        err = TCPSend(&sock, buf, sizeof(buf), &manager);
        if(err != 0) {
          printf("error at TCPSend %d | %s\n", err, strerror(err));
          exit(1);
        }
        (void) getc(stdin);
        break;
      }
      case 'l': {
        struct NETServer serv;
        struct NETConnManager manager;
        err = InitConnManager(&manager, 5);
        if(err != 0) {
          printf("error at InitConnectionManager %d | %s\n", err, strerror(err));
          exit(1);
        }
        err = SyncTCP_GAIListen(argv[3], argv[4], IPv4, &serv);
        if(err < -1) {
          printf("error at SyncTCP_GAIListen %d | %s\n", err, strerror(err));
          exit(1);
        } else if(err == -1) {
          puts("no address succeeded at SyncTCP_GAIListen");
          exit(1);
        }
        serv.onconnection = onconnection;
        serv.onerror = serveronerror;
        serv.manager = &manager;
        puts("SyncTCP_GAIListen succeeded");
        err = AddServer(&manager, serv);
        if(err != 0) {
          printf("error at AddServer %d | %s\n", err, strerror(err));
          exit(1);
        }
        puts("AddServer succeeded");
        (void) getc(stdin);
        break;
      }
      default: {
        puts("Option not recognized. The available options: l(isten) <host> <port>, c(onnect) <host> <port>");
        break;
      }
    }
  } else {
    puts("Option not recognized. The available options: a(sync), s(ync)");
  }
  return 0;
}*/