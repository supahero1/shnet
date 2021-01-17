#include "check.h"
#include "def.h"

#include "distr/distr.h"
#include "net/net.h"

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

// source /opt/rh/gcc-toolset-9/enable
// cc main.c distr/distr.c net/net.c -o main -O3 -Wall -pthread -lanl -Wno-missing-braces -Wmissing-field-initializers -D_GNU_SOURCE

void cbc(struct NETSocket* res, int sfd) {
  if(sfd < 0) {
    printf("error code: %d\n", sfd);
  } else {
    printf("sfd: %d\n", sfd);
  }
  switch(sfd) {
    case -1: {
      printf("getaddrinfo error: %s\n", gai_strerror(errno));
      break;
    }
    case -2: {
      printf("connect error: %s\n", strerror(errno));
      break;
    }
    case -3: {
      puts("no addresses succeeded to connect :(");
      break;
    }
    default: {
      puts("sucess connecting a socket");
      break;
    }
  }
}

void cbl(struct NETSocket* res, int sfd) {
  if(sfd < 0) {
    printf("error code: %d\n", sfd);
  } else {
    printf("sfd: %d\n", sfd);
  }
  switch(sfd) {
    case -1: {
      printf("getaddrinfo error: %s\n", gai_strerror(errno));
      break;
    }
    case -2: {
      printf("bind error: %s\n", strerror(errno));
      break;
    }
    case -3: {
      printf("listen error: %s\n", strerror(errno));
      break;
    }
    case -4: {
      puts("no addresses succeeded to bind/listen :(");
      break;
    }
    default: {
      puts("sucess setting up a listening socket");
      break;
    }
  }
}

int main(int argc, char **argv) {
  int err;
  /*struct WorkerCluster cluster = WorkerCluster(DISTR_ALWAYS);
  cluster.onready = workercluster_ready;
  err = PopulateCluster(&cluster, 4);
  if(err != 0) {
    puts("0");
    exit(1);
  }
  void* work[500000];
  for(uint64_t i = 0; i < 250000; ++i) {
    work[i << 1] = lmfao;
    work[(i << 1) + 1] = NULL;
  }
  err = Work(&cluster, work, 500000);
  if(err != 0) {
    puts("1");
    exit(1);
  }
  puts("done, waiting");*/
  //struct addrinfo* res;
  //err = BandagedGetAddrInfo(&res, argv[1], argv[2], AF_INET);
  if(argc < 2) {
    puts("Minimum amount of arguments is 1.");
    return 1;
  }
  switch(argv[1][0]) {
    case 'c': {
      struct AsyncNETArray a = (struct AsyncNETArray) {
        .count = 1,
        .type = NET_GAI,
        .infos = (union GAIInfo) {
          .gai = &((struct AsyncGAI) {
            .hostname = argv[2],
            .service = argv[3],
            .flags = AF_INET,
            .callback = cbc
          })
        }
      };
      err = AsyncTCPConnect(&a);
      if(err != 0) {
        printf("error at AsyncTCPConnect %d | %s\n", err, strerror(err));
        exit(1);
      } else {
        puts("AsyncTCPConnect succeeded, now waiting");
      }
      (void) getc(stdin);
      break;
    }
    case 'l': {
      struct AsyncNETArray a = (struct AsyncNETArray) {
        .count = 1,
        .type = NET_GAI,
        .infos = (union GAIInfo) {
          .gai = &((struct AsyncGAI) {
            .hostname = NULL,
            .service = argv[2],
            .flags = AF_INET,
            .callback = cbl
          })
        }
      };
      err = AsyncTCPListen(&a);
      if(err != 0) {
        printf("error at AsyncTCPListen %d | %s\n", err, strerror(err));
        exit(1);
      } else {
        puts("AsyncTCPListen succeeded, now waiting");
      }
      (void) getc(stdin);
      break;
    }
    default: {
      puts("Option not recognized. The available options: l <port>, c <host> <port>");
      break;
    }
  }
  return 0;
}
