#include "tests.h"

#include <limits.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>
#include <stdatomic.h>

#include <shnet/tcp.h>
#include <shnet/time.h>

#define min(x, y) (((x) < (y)) ? (x) : (y))
#define max(x, y) (((x) > (y)) ? (x) : (y))

int method = -1;
int message_size = 4096 << 7;

char* buf;
_Atomic uint64_t total = 0;
_Atomic int test_is_over = 0;

struct async_loop* async_loops;
int async_loops_num;
_Atomic int async_loop_num = 0;

struct addrinfo* res;

void socket_onevent(struct tcp_socket* socket, enum tcp_event event) {
  switch(event) {
    case tcp_open: {
      if(socket->core.server && method) {
        atomic_fetch_add_explicit(&total, 1, memory_order_acq_rel);
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
        atomic_fetch_add_explicit(&total, read, memory_order_acq_rel);
        if(read == 0 || atomic_load_explicit(&test_is_over, memory_order_acquire)) {
          break;
        }
        tcp_send(socket, &((struct data_frame) {
          .data = buf,
          .len = read,
          .read_only = 1,
          .dont_free = 1
        }));
      }
      break;
    }
    case tcp_close: {
      tcp_socket_free(socket);
      break;
    }
    case tcp_free: {
      if(socket->core.socket) {
        if(atomic_load_explicit(&test_is_over, memory_order_acquire)) {
          test_wake();
          break;
        }
        /* Can fail, no assert() */
        tcp_socket(socket, &((struct tcp_socket_options) {
          .info = res
        }));
      }
      break;
    }
    default: break;
  }
}

struct tcp_socket* server_onevent(struct tcp_server* server, enum tcp_event event, struct tcp_socket* socket) {
  switch(event) {
    case tcp_open: {
      socket->on_event = socket_onevent;
      break;
    }
    case tcp_close: {
      tcp_server_free(server);
      break;
    }
    case tcp_free: {
      test_wake();
      break;
    }
    default: break;
  }
  return socket;
}

void stop_test(void* da) {
  atomic_store_explicit(&test_is_over, 1, memory_order_release);
  test_wake();
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
    "                    share their async_loops with this\n"
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
  int shared_async_loop = 0;
  int time_for_test = 500;
  unsigned short port = 5000;
  
  test_seed_random();
  
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
        shared_async_loop = 1;
      } else if(!strcmp(argv[i] + 1, "p") || !strcmp(argv[i] + 1, "-port")) {
        port = atoi(argv[++i]);
        if(port < 1) {
          puts("Port must be greater than 0");
          return -1;
        }
      } else if(strlen(argv[i]) > 1 && argv[i][1] != '-') {
        printf("\"%s\" is not recognized as an option. Try -h or --help for the list of available options.\n", argv[i]);
      }
    }
  }
  if(method == -1) {
    print_help();
    return -1;
  }
  
  if(method == 0) {
    buf = calloc(1, message_size);
    assert(buf);
  }
  
  char _port[6];
  assert(sprintf(_port, "%hu", port)>0);
  const struct addrinfo hints = net_get_addr_struct(net_family_ipv4, net_sock_stream, net_proto_tcp, 0);
  res = net_get_address("127.0.0.1", _port, &hints);
  assert(res);
  
  printf(
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
  , method ? "connstress" : "bandwidth", clients_num, servers_num, client_restrict_thrds, server_restrict_thrds, shared_async_loop ? "yes" : "no", message_size, time_for_test, port);
  
  const int client_async_loops = min(clients_num, client_restrict_thrds);
  const int server_async_loops = min(servers_num, server_restrict_thrds);
  async_loops_num = shared_async_loop ? max(client_async_loops, server_async_loops) : (client_async_loops + server_async_loops);
  async_loops = calloc(async_loops_num, sizeof(struct async_loop));
  for(int i = 0; i < async_loops_num; ++i) {
    assert(!tcp_async_loop(&async_loops[i]));
    assert(!async_loop_start(&async_loops[i]));
  }
  
  /* TCP server setup */
  struct tcp_server* servers = calloc(servers_num, sizeof(struct tcp_server));
  assert(servers);
  for(int i = 0; i < servers_num; ++i) {
    servers[i].on_event = server_onevent;
    servers[i].loop = &async_loops[shared_async_loop ? (i % async_loops_num) : (i % server_async_loops)];
    assert(!tcp_server(&servers[i], &((struct tcp_server_options) {
      .info = res,
      .backlog = clients_num << 2
    })));
  }
  
  /* TCP socket setup */
  struct tcp_socket* sockets = calloc(clients_num, sizeof(struct tcp_socket));
  assert(sockets);
  for(int i = 0; i < clients_num; ++i) {
    sockets[i].on_event = socket_onevent;
    sockets[i].loop = &async_loops[shared_async_loop ? (i % async_loops_num) : (server_async_loops + (i % client_async_loops))];
    assert(!tcp_socket(&sockets[i], &((struct tcp_socket_options) {
      .info = res
    })));
  }
  
  /* Time setup to stop the test */
  struct time_timers timers = {0};
  assert(!time_timers(&timers));
  assert(!time_timers_start(&timers));
  assert(!time_add_timeout(&timers, &((struct time_timeout) {
    .time = time_get_ms(time_for_test),
    .func = stop_test
  })));
  if(method == 0) {
    for(int i = 0; i < clients_num; ++i) {
      assert(!tcp_send(&sockets[i], &((struct data_frame) {
        .data = buf,
        .len = message_size,
        .read_only = 1,
        .dont_free = 1
      })));
    }
  }
  test_wait();
  /* The test is over */
  const uint64_t sum = atomic_load_explicit(&total, memory_order_acquire);
  if(method == 0) {
    printf("The results:\n"
    "Bytes sent: %lu\n"
    "Throughput: %lu b/s, %lu mb/s\n",
    sum,
    sum * 1000U / time_for_test,
    sum / (time_for_test * 1000U)
    );
  } else if(method == 1) {
    printf("The results:\n"
    "Conns made: %lu\n"
    "Throughput: %lu conn/s, %lu kc/s\n",
    sum,
    sum * 1000U / time_for_test,
    sum / time_for_test
    );
  }
  if(method == 0) {
    for(int i = 0; i < clients_num; ++i) {
      tcp_socket_force_close(&sockets[i]);
    }
  }
  /* Wait for clients */
  for(int i = 0; i < clients_num; ++i) {
    test_wait();
  }
  /* Close the servers */
  for(int i = 0; i < servers_num; ++i) {
    assert(!tcp_server_close(&servers[i]));
    test_wait();
  }
  /* Close the async loops */
  for(int i = 0; i < async_loops_num; ++i) {
    async_loop_stop(&async_loops[i]);
    async_loop_free(&async_loops[i]);
  }
  /* Free the rest */
  free(async_loops);
  free(sockets);
  free(servers);
  time_timers_stop(&timers);
  time_timers_free(&timers);
  net_free_address(res);
  if(method == 0) {
    free(buf);
  }
  return 0;
}