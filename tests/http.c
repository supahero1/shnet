#include "tests.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <shnet/http.h>

#define try(a) if((a) != NULL) printf("%s\n", (a));

void onresponse(struct http_socket* socket, struct http_message* message, uint32_t idx) {
  printf("got a response to request nr %u, status code %u\n", idx, message->status_code);
}

void onnomem(void) {
  puts("no mem");
  exit(1);
}

void onclose(struct http_socket* socket) {
  puts("socket closed");
}

int main() {
  printf_debug("Testing http:", 1);
  
  struct addrinfo* info = net_get_address("diep.io", "80", net_get_addr_struct(ipv4, stream_socktype, tcp_protocol, numeric_service));
  if(info == NULL) {
    puts("no addresses");
    return 1;
  }
  
  int err = http("https://diep.io", info, &((struct http_request) {
    .request = (struct http_message) {
      
    },
    .response_settings = (struct http_parser_settings) {
      
    }
  }), &((struct http_settings) {
    .epoll = NULL,
    .manager = NULL,
    .ctx = NULL,
    .timeout_after = 0,
    .read_buffer_growth = 65536
  }), &((struct http_callbacks) {
    .onresponse = onresponse,
    .onnomem = onnomem,
    .onclose = onclose
  }), 1);
  
  sleep(9999);
  TEST_PASS;
  return 0;
}