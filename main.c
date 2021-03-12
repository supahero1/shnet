#include "check.h"
#include "def.h"

#include "distr/distr.h"
#include "net/net_avl.h"
#include "net/net.h"

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

void event_handler(int sfd, uint32_t events) {
  printf("socket %d received the following events: ", sfd);
  if((events & EPOLLIN) != 0) {
    printf("EPOLLIN ");
  }
  if((events & EPOLLPRI) != 0) {
    printf("EPOLLPRI ");
  }
  if((events & EPOLLOUT) != 0) {
    printf("EPOLLOUT ");
  }
  if((events & EPOLLMSG) != 0) {
    printf("EPOLLMSG ");
  }
  if((events & EPOLLERR) != 0) {
    printf("EPOLLERR ");
    int err;
    getsockopt(sfd, SOL_SOCKET, SO_ERROR, &err, NULL);
    printf("(btw the error is: %s) ", strerror(err));
  }
  if((events & EPOLLHUP) != 0) {
    printf("EPOLLHUP ");
  }
  if((events & EPOLLRDHUP) != 0) {
    printf("EPOLLRDHUP ");
  }
  printf("\n");
  if((events & EPOLLHUP) != 0) {
    puts("quitting bc connection is ded af");
    exit(1);
  }
}

int main(int argc, char **argv) {
  int err;
  if(argc < 2) {
    puts("Minimum amount of arguments is 1.");
    return 1;
  }
  switch(argv[1][0]) {
    case 'c': {
      struct NETSocket sock;
      struct NETConnectionManager manager;
      err = InitConnectionManager(&manager, 1);
      printf("result %d\n", err);
      if(err != 0) {
        printf("error lol %d | %s\n", err, strerror(err));
        exit(1);
      }
      /*struct TIStorage ti = TIStorage(DISTR_ALWAYS);
      err = DeployTimeout(&ti);
      if(err != 0) {
        puts("deploy err");
        exit(1);
      }*/
      err = SyncTCP_GAIConnect(argv[2], argv[3], &sock);
      if(err < -1) {
        printf("error at SyncTCP_GAIConnect %d | %s\n", err, strerror(err));
        exit(1);
      } else if(err == -1) {
        puts("no address succeeded at SyncTCP_GAIConnect");
        exit(1);
      }
      puts("SyncTCP_GAIConnect succeeded11");
      err = AddSocket(&manager, sock.sfd, event_handler);
      if(err != 0) {
        printf("error at AddSocket %d | %s\n", err, strerror(err));
        exit(1);
      }
      puts("AddSocket succeeded11");
      uint8_t buf[] = { 'G', 'E', 'T', ' ', '/', ' ', 'H', 'T', 'T', 'P', '/', '1', '.', '0' };
      printf("sent: %ld\n", send(sock.sfd, buf, 14, MSG_NOSIGNAL));
      (void) getc(stdin);
      break;
    }
    case 'l': {
      
      break;
    }
    default: {
      puts("Option not recognized. The available options: l <port>, c <host> <port>");
      break;
    }
  }
  return 0;
}