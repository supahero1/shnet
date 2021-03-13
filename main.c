#include "check.h"
#include "def.h"

#include "distr/distr.h"
#include "net/net_base.h"
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

void onmessage(struct NETSocket socket) {
  puts("onmessage()");
  char buffer[50000];
  memset(buffer, 0, 50000);
  long bytes = recv(socket.sfd, buffer, 50000, 0);
  printf("nice got %ld bytes\n", bytes);
  printf("%s\n", buffer);
}

void onclose(struct NETSocket socket) {
  puts("onclose()");
}

void onerror(struct NETSocket socket) {
  printf("got a socket error when executing some function: %s\n", strerror(errno));
}

void onsent(struct NETSocket socket) {
  puts("the socket sent all data we wanted it to send");
}

int main(int argc, char** argv) {
  int err;
  printf("%ld %ld %ld\n", sizeof(struct NETSocket), sizeof(struct net_avl_node), sizeof(struct sockaddr));
  if(argc < 2) {
    puts("Minimum amount of arguments is 1.");
    return 1;
  }
  switch(argv[1][0]) {
    case 'c': {
      struct NETSocket sock[100];
      struct NETConnectionManager manager;
      err = InitConnectionManager(&manager, 5);
      if(err != 0) {
        printf("error at InitConnectionManager %d | %s\n", err, strerror(err));
        exit(1);
      }
      for(int i = 0; i < 10; ++i) {
        err = SyncTCP_GAIConnect(argv[2], argv[3], IPv4, &sock[i]);
        if(err < -1) {
          printf("error at SyncTCP_GAIConnect %d | %s\n", err, strerror(err));
          exit(1);
        } else if(err == -1) {
          puts("no address succeeded at SyncTCP_GAIConnect");
          exit(1);
        }
        sock[i].onmessage = onmessage;
        sock[i].onclose = onclose;
        sock[i].onerror = onerror;
        sock[i].onsent = onsent;
        puts("SyncTCP_GAIConnect succeeded");
        err = AddSocket(&manager, sock[i]);
        if(err != 0) {
          printf("error at AddSocket %d | %s\n", err, strerror(err));
          exit(1);
        }
        puts("AddSocket succeeded");
        /*char buf[] = "GET /index.html HTTP/1.1\r\nConnection: close\r\n\r\n";
        err = TCPSend(&sock[i], buf, sizeof(buf));
        if(err != 0) {
          printf("error at TCPSend %d | %s\n", err, strerror(err));
          exit(1);
        }*/
      }
      char buf[] = "GET /index.html HTTP/1.1\r\nConnection: close\r\n\r\n";
      for(int i = 0; i < 10; ++i) {
        err = TCPSend(&sock[i], buf, sizeof(buf));
        if(err != 0) {
          printf("error at TCPSend %d | %s\n", err, strerror(err));
          exit(1);
        }
      }
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