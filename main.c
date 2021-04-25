//#include "net/net.h"
//#include "net/http.h"

//#define NET_DEBUG

#include "src/debug.h"

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

/*void FreeAcceptThreadPoolWrapper(void* a) {
  puts("da free");
  FreeAcceptThreadPool((struct NETAcceptThreadPool*) a);
}

void threadpoolonstart(struct NETAcceptThreadPool* a) {
  puts("NETAcceptThreadPool started");
}

void threadpoolonerror(struct NETAcceptThreadPool* pool, int sfd) {
  printf("got a threadpool error (sfd %d): %s\n", sfd, strerror(errno));
  FreeAcceptThreadPool(pool);
}

void threadpoolonstop(struct NETAcceptThreadPool* a) {
  puts("NETAcceptThreadPool stopped");
}

void netconnmanageronstart(struct NETConnManager* a) {
  puts("NetConnManager started");
}

void onmessage(struct NETSocket* socket) {
  puts("onmessage()");
  char buffer[50000];
  memset(buffer, 0, 50000);
  long bytes = recv(socket->sfd, buffer, 50000, 0);
  printf("nice got %ld bytes\n", bytes);
  printf("%s\n", buffer);
  printf("\nnow parsed:\n");
  struct HTTP_response response;
  struct HTTP_settings settings = HTTP_default_settings();
  int err = HTTPv1_1_response_parser((uint8_t*) buffer, (uint32_t) bytes, 0, &response, &settings, NULL);
  char* path_space = malloc(response.reason_phrase_length + 1);
  memcpy(path_space, response.reason_phrase, response.reason_phrase_length);
  path_space[response.reason_phrase_length] = 0;
  printf("%s\n", HTTP_strerror(err));
  if(err == HTTP_VALID) {
    printf("status code: %u\n"
           "reason phrase: %s\n"
           "reason phrase length: %u\n"
           "header amount: %u\n",
           response.status_code, path_space, response.reason_phrase_length, response.header_amount);
    for(uint32_t i = 0; i < response.header_amount; ++i) {
      path_space = realloc(path_space, response.headers[i].name_length + 1);
      memcpy(path_space, response.headers[i].name, response.headers[i].name_length);
      path_space[response.headers[i].name_length] = 0;
      printf("%s ", path_space);
      path_space = realloc(path_space, response.headers[i].value_length + 1);
      memcpy(path_space, response.headers[i].value, response.headers[i].value_length);
      path_space[response.headers[i].value_length] = 0;
      printf("- %s\n", path_space);
    }
  }
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
      printf("sfd is %d\n", sfd);
      int err;
      struct NETConnManager manager;
      manager.onstart = netconnmanageronstart;
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
      */
      /*gnutls_session_t session;
      err = gnutls_init(&session, GNUTLS_CLIENT | GNUTLS_NONBLOCK | GNUTLS_NO_SIGNAL);
      printf("gnutls_init(): %d\n", err);
      if(err < 0) {
        exit(1);
      }*/
      /*
      struct HTTP_header headers[] = {
        {
          .name = "Connection",
          .name_length = strlen("connection"),
          .value = "Close",
          .value_length = strlen("close")
        },
        {
          .name = "Host",
          .name_length = strlen("Host"),
          .value = "wikipedia.com",
          .value_length = strlen("wikipedia.com")
        },
        {
          .name = "Origin",
          .name_length = strlen("Origin"),
          .value = "http://wikipedia.com",
          .value_length = strlen("http://wikipedia.com")
        },
        {
          .name = "User-Agent",
          .name_length = strlen("User-Agent"),
          .value = "Mozilla/5.0 (X11; Ubuntu; Linux x86_64; rv:87.0) Gecko/20100101 Firefox/87.0",
          .value_length = strlen("Mozilla/5.0 (X11; Ubuntu; Linux x86_64; rv:87.0) Gecko/20100101 Firefox/87.0")
        },
        {
          .name = "Accept-Encoding",
          .name_length = strlen("Accept-Encoding"),
          .value = "gzip",
          .value_length = strlen("gzip")
        }
      };
      char* buffer = NULL;
      struct HTTP_request request = (struct HTTP_request) {
        .headers = headers,
        .body = NULL,
        .header_amount = 5,
        .body_length = 0,
        .method = HTTP_GET,
        .path = "/index.html",
        .path_length = strlen("/index.html")
      };
      err = HTTP_create_request(&buffer, 4096, 0, &request);
      if(err == 0) {
        puts("out of memory :(");
        exit(1);
      }
      puts("HTTP_create_request() succeeded");
      char* lmfao = malloc(err + 1);
      if(lmfao == NULL) {
        puts("ehh");
        exit(1);
      }
      (void) memcpy(lmfao, buffer, err);
      lmfao[err] = 0;
      printf("generated request:\n%s", lmfao);
      err = TCPSend(socket, buffer, err, &manager);
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
      printf("sfd is %d\n", sfd);
      int err;
      struct NETConnManager manager;
      manager.onstart = netconnmanageronstart;
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
      struct NETAcceptThreadPool pool;
      pool.onstart = threadpoolonstart;
      pool.onerror = threadpoolonerror;
      pool.onstop = threadpoolonstop;
      pool.server = server;
      err = InitAcceptThreadPool(&pool, 10, 1);
      if(err != 0) {
        printf("error at InitAcceptThreadPool %d | %s\n", err, strerror(err));
        exit(1);
      }
      puts("InitAcceptThreadPool succeeded");
      server->pool = &pool;
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
    freeaddrinfo(info);
  } else {
    printf("error at AsyncGetAddrInfo: %s\n", strerror(status));
    exit(1);
  }
}
*/
int main(int argc, char** argv) {
  NET_LOG("main() in");
  /*char req[] = "CONNECT /lmfao/lol?XD HTTP/1.1\r\nConnection: close\r\nUpgrade:  websocket\r\n\r\n\0";
  printf("parsing:\n\n%s", req);
  struct HTTP_request request;
  struct HTTP_settings settings = HTTP_default_settings();
  int err = HTTPv1_1_request_parser((uint8_t*) req, strlen(req), HTTP_ALLOW_REPETITIVE_HEADER_WHITESPACE, &request, &settings, NULL);
  char* path_space = malloc(request.path_length + 1);
  memcpy(path_space, request.path, request.path_length);
  path_space[request.path_length] = 0;
  printf("%s\n", HTTP_strerror(err));
  if(err == HTTP_VALID) {
    printf("path: %s\n"
           "path length: %u\n"
           "header amount: %u\n",
           path_space, request.path_length, request.header_amount);
    for(uint32_t i = 0; i < request.header_amount; ++i) {
      path_space = realloc(path_space, request.headers[i].name_length + 1);
      memcpy(path_space, request.headers[i].name, request.headers[i].name_length);
      path_space[request.headers[i].name_length] = 0;
      printf("%s ", path_space);
      path_space = realloc(path_space, request.headers[i].value_length + 1);
      memcpy(path_space, request.headers[i].value, request.headers[i].value_length);
      path_space[request.headers[i].value_length] = 0;
      printf("- %s\n", path_space);
    }
  }
  free(path_space);*/
  
  /*char res[] = "HTTP/1.1 200 OK\r\nConnection:close\r\nUpgrade:websocket\r\n\r\n\0";
  printf("parsing:\n\n%s", res);
  struct HTTP_response response;
  struct HTTP_settings settings = HTTP_default_settings();
  int err = HTTPv1_1_response_parser((uint8_t*) res, strlen(res), 0, &response, &settings, NULL);
  char* path_space = malloc(response.reason_phrase_length + 1);
  memcpy(path_space, response.reason_phrase, response.reason_phrase_length);
  path_space[response.reason_phrase_length] = 0;
  printf("%s\n", HTTP_strerror(err));
  if(err == HTTP_VALID) {
    printf("status code: %u\n"
           "reason phrase: %s\n"
           "reason phrase length: %u\n"
           "header amount: %u\n",
           response.status_code, path_space, response.reason_phrase_length, response.header_amount);
    for(uint32_t i = 0; i < response.header_amount; ++i) {
      path_space = realloc(path_space, response.headers[i].name_length + 1);
      memcpy(path_space, response.headers[i].name, response.headers[i].name_length);
      path_space[response.headers[i].name_length] = 0;
      printf("%s ", path_space);
      path_space = realloc(path_space, response.headers[i].value_length + 1);
      memcpy(path_space, response.headers[i].value, response.headers[i].value_length);
      path_space[response.headers[i].value_length] = 0;
      printf("- %s\n", path_space);
    }
  }
  struct HTTP_header* h = HTTP_get_header(response.headers, response.header_amount, "upgRaDE", strlen("Upgrade"));
  printf("%p %p\n", (void*) h, (void*) response.headers);
  free(path_space);*/
  
  /*char* buffer = NULL;
  char body[] = "This is a HTTP body I have just written. HTTP blah blah.";
  struct HTTP_header headers[3] = {
    {
      .name = "connection",
      .name_length = strlen("connection"),
      .value = "close",
      .value_length = strlen("close")
    },
    {
      .name = "upgrade",
      .name_length = strlen("upgrade"),
      .value = "websocket",
      .value_length = strlen("websocket")
    },
    {
      .name = "accept-language",
      .name_length = strlen("accept-language"),
      .value = "pl_PL",
      .value_length = strlen("pl_PL")
    }
  };*/
  
  /*struct HTTP_request request = (struct HTTP_request) {
    .headers = headers,
    .body = (uint8_t*) body,
    .header_amount = 3,
    .body_length = strlen(body),
    .method = HTTP_CONNECT,
    .path = "/submit/request",
    .path_length = strlen("/submit/request")
  };
  int err = HTTP_create_request(&buffer, 4096, 0, &request);*/
  
  /*struct HTTP_response response = {
    .headers = headers,
    .body = (uint8_t*) body,
    .header_amount = 3,
    .body_length = strlen(body),
    .status_code = HTTP_OK,
    .reason_phrase = "OK",
    .reason_phrase_length = strlen("OK")
  };
  int err = HTTP_create_response(&buffer, 4096, 0, &response);
  if(err == 0) {
    puts("error lmfao");
    exit(1);
  }
  printf("HTTP_create_xxx succeeded, size of output: %u\n", err);
  char* lmfao = malloc(err + 1);
  if(lmfao == NULL) {
    puts("ehh");
    exit(1);
  }
  (void) memcpy(lmfao, buffer, err);
  lmfao[err] = 0;
  printf("output:\n%s", lmfao);
  return 0;*/
  
  
  
  
  int err;
  if(argc < 5) {
    puts("Minimum amount of arguments is 4.");
    NET_LOG("argc check failed");
    return 1;
  }
  NET_LOG("argc check passed");
  /*
  if(argv[1][0] == 'a') {
    switch(argv[2][0]) {
      case 'c': {
        err = AsyncGetAddrInfo(&((struct ANET_GAIArray) {
          .arr = &((struct ANET_GAILookup) {
            .handler = asyncgai,
            .hostname = argv[3],
            .service = argv[4],
            .flags = NET_IPv4
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
            .flags = NET_IPv4 | NET_SERVER
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
        manager.onstart = netconnmanageronstart;
        err = InitConnManager(&manager, 5);
        if(err != 0) {
          printf("error at InitConnectionManager %d | %s\n", err, strerror(err));
          exit(1);
        }
        puts("InitConnManager succeeded");
        err = SyncTCP_GAIConnect(argv[3], argv[4], NET_IPv4, &sock);
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
        //manager.onstart = netconnmanageronstart;
        err = InitConnManager(&manager, 5);
        if(err != 0) {
          printf("error at InitConnectionManager %d | %s\n", err, strerror(err));
          exit(1);
        }
        err = SyncTCP_GAIListen(argv[3], argv[4], NET_IPv4, &serv);
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
        struct NETAcceptThreadPool pool;
        pool.onstart = threadpoolonstart;
        pool.onerror = threadpoolonerror;
        pool.onstop = threadpoolonstop;
        pool.server = &serv;
        err = InitAcceptThreadPool(&pool, 10, 1);
        if(err != 0) {
          printf("error at InitAcceptThreadPool %d | %s\n", err, strerror(err));
          exit(1);
        }
        puts("InitAcceptThreadPool succeeded");
        serv.pool = &pool;
        /*struct Timeout timeout = Timeout();
        struct TimeoutObject work = (struct TimeoutObject) {
          .time = GetTime(1000000000UL * 1),
          .func = FreeAcceptThreadPoolWrapper,
          .data = &pool
        };
        int err = StartTimeoutThread(&timeout, TIME_ALWAYS);
        if(err != 0) {
          puts("timeout");
          exit(1);
        }
        err = SetTimeout(&timeout, &work, 1);
        if(err != 0) {
          puts("SetTimeout");
          exit(1);
        }*\/
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
  }*/
  return 0;
}