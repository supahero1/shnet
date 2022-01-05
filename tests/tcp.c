#include "tests.h"

#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>
#include <semaphore.h>

#include <shnet/tcp.h>
#include <shnet/time.h>
#include <shnet/error.h>

#define min(x, y) (((x) < (y)) ? (x) : (y))
#define max(x, y) (((x) > (y)) ? (x) : (y))

sem_t sem;
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

int method = -1;
int message_size = 4096 << 7;

unsigned char* buf;
_Atomic uint64_t total = 0;
_Atomic int test_is_over = 0;

struct net_epoll* epolls;
int epolls_num;
_Atomic int epoll_num = 0;

void epoll_startup(void* nil) {
  const int id = atomic_fetch_add(&epoll_num, 1);
  (void) net_epoll_thread(&epolls[id]);
}

int socket_onevent(struct tcp_socket* socket, enum tcp_event event) {
  switch(event) {
    case tcp_open: {
      if(method == 0) {
        sem_post(&sem);
      } else if(method == 1 && socket->server != NULL) {
        atomic_fetch_add(&total, 1);
        tcp_socket_close(socket);
      }
      break;
    }
    case tcp_data: {
      if(method == 0) {
        errno = 0;
        uint64_t read = 0;
        uint64_t r;
        do {
          r = tcp_read(socket, buf, message_size);
          read += r;
        } while(r != 0 && errno == 0);
        atomic_fetch_add(&total, read);
        if(read == 0 || atomic_load(&test_is_over) == 1) {
          break;
        }
        tcp_send(socket, buf, read, data_read_only | data_dont_free);
      }
      break;
    }
    case tcp_close: {
      tcp_socket_free(socket);
      break;
    }
    case tcp_destruction: {
      if(method == 0 && socket->server != NULL) {
        sem_post(&sem);
      }
      break;
    }
    case tcp_free: {
      sem_post(&sem);
      break;
    }
    default: break;
  }
  return 0;
}

int server_onevent(struct tcp_server* server, enum tcp_event event, struct tcp_socket* socket, const struct sockaddr* addr) {
  switch(event) {
    case tcp_creation: {
      socket->on_event = socket_onevent;
      break;
    }
    case tcp_close: {
      tcp_server_free(server);
      break;
    }
    case tcp_free: {
      sem_post(&sem);
      break;
    }
    default: break;
  }
  return 0;
}

void stop_test(void* da) {
  atomic_store(&test_is_over, 1);
  pthread_mutex_unlock(&mutex);
}

int onerr(int code) {
  return -1;
}

void print_help(void) {
  puts(
    "Usage:\n"
    "-h --help           Prints this message.\n"
    "-b --bandwidth      Runs the bandwidth test.\n"
    "-c --connstress     Runs the connection stress\n"
    "                    test, which spawns clients\n"
    "                    and mass connects them to\n"
    "                    prepared server(s). It\n"
    "                    measures connections/sec.\n"
    "-C --clients        Number of clients.\n"
    "-S --servers        Number of servers.\n"
    "-m --msgsize        Sets the message size used\n"
    "                    in the bandwidth test. The\n"
    "                    default is 4096 << 7 (524kb).\n"
    "-t --time           Sets how long the test is\n"
    "                    going to be run for. Longer\n"
    "                    gives more accurate results.\n"
    "                    Measured in milliseconds.\n"
    "-r --crestrict      Restricts clients to given\n"
    "                    number of thrds. By default\n"
    "                    each client and server\n"
    "                    receives their own thread\n"
    "                    to work on (capped at 100).\n"
    "-R --srestrict      Above, but for servers.\n"
    "-s --shared         Clients and servers will\n"
    "                    share their epolls with this\n"
    "                    setting present. By default,\n"
    "                    clients and servers receive\n"
    "                    separate threads to work on.\n"
    "                    Using this, one can make all\n"
    "                    sockets to run on only 1 core.\n"
    "-p --port           Sets local port for the tests.\n"
    "Specifying the test type is required. The rest is\n"
    "optional."
  );
}

int main(int argc, char** argv) {
  int clients_num = 1;
  int servers_num = 1;
  int client_restrict_thrds = 100;
  int server_restrict_thrds = 100;
  int shared_epoll = 0;
  int time_for_test = 200;
  unsigned short port = 5000;
  
  if(argc == 1) {
    print_help();
    return -1;
  }
  
  for(int i = 1; i < argc; ++i) {
    if(argv[i][0] == '-') {
      if(!strcmp(argv[i] + 1, "h") || !strcmp(argv[i] + 1, "-help")) {
        print_help();
        return 0;
      } else if(!strcmp(argv[i] + 1, "b") || !strcmp(argv[i] + 1, "-bandwidth")) {
        method = 0;
      } else if(!strcmp(argv[i] + 1, "c") || !strcmp(argv[i] + 1, "-connstress")) {
        method = 1;
      } else if(!strcmp(argv[i] + 1, "C") || !strcmp(argv[i] + 1, "-clients")) {
        clients_num = atoi(argv[++i]);
        if(clients_num < 1) {
          puts("The number of clients can't be less than 1.");
          return -1;
        }
      } else if(!strcmp(argv[i] + 1, "S") || !strcmp(argv[i] + 1, "-servers")) {
        servers_num = atoi(argv[++i]);
        if(servers_num < 1) {
          puts("The number of servers can't be less than 1.");
          return -1;
        }
      } else if(!strcmp(argv[i] + 1, "m") || !strcmp(argv[i] + 1, "-msgsize")) {
        message_size = atoi(argv[++i]);
        if(message_size < 1) {
          puts("Message size can't be less than 1.");
          return -1;
        }
      } else if(!strcmp(argv[i] + 1, "t") || !strcmp(argv[i] + 1, "-time")) {
        time_for_test = atoi(argv[++i]);
        if(time_for_test < 1) {
          puts("Test's time can't be less than 1ms.");
          return -1;
        }
      } else if(!strcmp(argv[i] + 1, "r") || !strcmp(argv[i] + 1, "-crestrict")) {
        client_restrict_thrds = atoi(argv[++i]);
        if(client_restrict_thrds < 1) {
          puts("Can't impose a core limit less than 1.");
          return -1;
        }
      } else if(!strcmp(argv[i] + 1, "R") || !strcmp(argv[i] + 1, "-srestrict")) {
        server_restrict_thrds = atoi(argv[++i]);
        if(server_restrict_thrds < 1) {
          puts("Can't impose a core limit less than 1.");
          return -1;
        }
      } else if(!strcmp(argv[i] + 1, "s") || !strcmp(argv[i] + 1, "-shared")) {
        shared_epoll = 1;
      } else if(!strcmp(argv[i] + 1, "p") || !strcmp(argv[i] + 1, "-port")) {
        port = atoi(argv[++i]);
      } else if(strlen(argv[i]) > 1 && argv[i][1] != '-') {
        printf("\"%s\" is not recognized as an option. Try -h or --help for the list of available options.\n", argv[i]);
      }
    }
  }
  if(method == -1) {
    print_help();
    return -1;
  }
  
  _debug(
    "The input:\n"
    "Method          : %s\n"
    "Num of clients  : %d\n"
    "Num of servers  : %d\n"
    "Client max thrds: %d\n"
    "Server max thrds: %d\n"
    "Shared threads  : %s\n"
    "Message size    : %d bytes\n"
    "Test time       : %dms\n"
    "Port            : %hu\n"
  , 1, method ? "connstress" : "bandwidth", clients_num, servers_num, client_restrict_thrds, server_restrict_thrds, shared_epoll ? "yes" : "no", message_size, time_for_test, port);
  
  srand(time_get_time());
  sem_init(&sem, 0, 0);
  pthread_mutex_lock(&mutex);
  handle_error = onerr;
  
  if(method == 0) {
    buf = calloc(1, message_size);
    if(buf == NULL) {
      TEST_FAIL;
    }
  }
  
  struct tcp_server_settings server_set = { clients_num << 2, clients_num << 2 };
  
  struct tcp_socket_settings sock_set = { 1, 1, 0, 0, 0, 1, 0 };
  
  char ppoorrtt[11];
  sprintf(ppoorrtt, "%hu", port);
  const struct addrinfo hints = net_get_addr_struct(ipv4, stream_socktype, tcp_protocol, 0);
  struct addrinfo* res = net_get_address("localhost", ppoorrtt, &hints);
  if(res == NULL) {
    _debug("net_get_address() err %d %s", 1, errno, strerror(errno));
    TEST_FAIL;
  }
  
  const int client_epolls = min(clients_num, client_restrict_thrds);
  const int server_epolls = min(servers_num, server_restrict_thrds);
  epolls_num = shared_epoll ? max(client_epolls, server_epolls) : (client_epolls + server_epolls);
  epolls = calloc(epolls_num, sizeof(struct net_epoll));
  for(int i = 0; i < epolls_num; ++i) {
    if(tcp_server_epoll(&epolls[i]) == -1) {
      TEST_FAIL;
    }
  }
  /* Getting fancy with thread management */
  struct threads epoll_manager = {0};
  if(threads_add(&epoll_manager, epoll_startup, NULL, epolls_num) == -1) {
    TEST_FAIL;
  }
  
  /* TCP server setup */
  struct tcp_server* servers = calloc(servers_num, sizeof(struct tcp_server));
  if(servers == NULL) {
    TEST_FAIL;
  }
  for(int i = 0; i < servers_num; ++i) {
    servers[i].on_event = server_onevent;
    servers[i].settings = &server_set;
    servers[i].epoll = &epolls[shared_epoll ? (i % epolls_num) : (i % server_epolls)];
    if(tcp_server(&servers[i], &((struct tcp_server_options) {
      .info = res,
      .hostname = "localhost",
      .port = ppoorrtt,
      .family = ipv4,
      .flags = 0
    })) != 0) {
      _debug("tcp_server() errno %d", 1, errno);
      TEST_FAIL;
    }
  }
  
  /* TCP socket setup */
  struct tcp_socket* sockets = calloc(clients_num, sizeof(struct tcp_socket));
  if(sockets == NULL) {
    TEST_FAIL;
  }
  for(int i = 0; i < clients_num; ++i) {
    sockets[i].on_event = socket_onevent;
    sockets[i].info = res;
    sockets[i].epoll = &epolls[shared_epoll ? (i % epolls_num) : (server_epolls + (i % client_epolls))];
    sockets[i].settings = sock_set;
    if(tcp_socket(&sockets[i], &((struct tcp_socket_options) {
      .hostname = "localhost",
      .port = ppoorrtt,
      .family = ipv4,
      .flags = 0,
      .static_hostname = 1,
      .static_port = 1
    })) != 0) {
      _debug("tcp_socket() errno %d", 1, errno);
      TEST_FAIL;
    }
  }
  
  /* Time setup to stop the test */
  struct time_manager manager = {0};
  if(time_manager(&manager) != 0) {
    TEST_FAIL;
  }
  if(time_manager_start(&manager) != 0) {
    TEST_FAIL;
  }
  if(method == 0) {
    for(int i = 0; i < (clients_num << 1); ++i) {
      sem_wait(&sem);
    }
  }
  if(time_manager_add_timeout(&manager, time_get_ms(time_for_test), stop_test, NULL, NULL) != 0) {
    TEST_FAIL;
  }
  if(method == 0) {
    for(int i = 0; i < clients_num; ++i) {
      tcp_send(&sockets[i], buf, message_size, data_read_only | data_dont_free);
    }
  }
  /* Wait for the timeout to expire, we will be sending data in the background */
  pthread_mutex_lock(&mutex);
  /* The test is over */
  const uint64_t sum = atomic_load(&total);
  atomic_store(&test_is_over, 1);
  if(method == 0) {
    _debug("The results:\n"
    "Bytes sent: %lu\n"
    "Throughput: %lu b/s, %lu mb/s",
    1,
    sum,
    sum * 1000U / time_for_test,
    sum / (time_for_test * 1000U)
    );
  } else if(method == 1) {
    _debug("The results:\n"
    "Conns made: %lu\n"
    "Throughput: %lu conn/s, %lu kc/s",
    1,
    sum,
    sum * 1000U / time_for_test,
    sum / time_for_test
    );
  }
  if(method == 1) {
    for(int i = 0; i < servers_num; ++i) {
      tcp_server_dont_accept_conn(&servers[i]);
    }
  }
  /* Close the sockets */
  for(int i = 0; i < clients_num; ++i) {
    tcp_socket_force_close(&sockets[i]);
  }
  for(int i = 0; i < clients_num * ((method ^ 1) + 1); ++i) {
    sem_wait(&sem);
  }
  /* Close the servers */
  for(int i = 0; i < servers_num; ++i) {
    if(tcp_server_shutdown(&servers[i]) != 0) {
      TEST_FAIL;
    }
    sem_wait(&sem);
  }
  /* Close the epolls */
  threads_shutdown(&epoll_manager);
  threads_free(&epoll_manager);
  for(int i = 0; i < epolls_num; ++i) {
    net_epoll_free(&epolls[i]);
  }
  /* Free the rest */
  pthread_mutex_unlock(&mutex);
  pthread_mutex_destroy(&mutex);
  sem_destroy(&sem);
  free(epolls);
  free(sockets);
  free(servers);
  time_manager_stop(&manager);
  time_manager_free(&manager);
  net_free_address(res);
  if(method == 0) {
    free(buf);
  }
  TEST_PASS;
  debug_free();
  return 0;
}