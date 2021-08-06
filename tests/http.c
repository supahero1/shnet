#include "tests.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <shnet/http.h>

#define WIDTH 50

void hex_print(const unsigned char* const data, const uint64_t len) {
  for(uint64_t i = 0; i < len; i += WIDTH) {
    for(uint64_t j = 0; j < WIDTH; ++j) {
      if(i + j >= len) {
        printf("   ");
      } else {
        printf("%02X ", data[i + j]);
      }
    }
    printf(" ");
    for(uint64_t j = 0; j < WIDTH; ++j) {
      if(i + j < len && data[i + j] > 31 && data[i + j] < 127) {
        printf("%c", data[i + j]);
      } else {
        printf(".");
      }
    }
    printf("\n");
  }
}

void onresponse(struct http_socket* socket, struct http_message* message) {
  printf("got a response to request nr %u, status code %u\n", socket->context.requests_used, message->status_code);
  if(message->body != NULL) {
    message->body[message->body_len] = 0;
    printf("the received message (len %lu):\n%s\n\n", message->body_len, message->body);
  } else {
    puts("no body");
  }
  if(socket->context.requests_used == socket->context.requests_size - 1) {
    TEST_PASS;
    exit(0);
  }
}

int onnomem(struct http_socket* socket) {
  puts("no mem");
  exit(1);
  return -1;
}

void onclose(struct http_socket* socket, int reason) {
  printf("socket closed, errno %d, reason %s\n", errno, http_str_close_reason(reason));
}

void sonresponse(struct https_socket* socket, struct http_message* message) {
  printf("got a response to request nr %u, status code %u\n", socket->context.requests_used, message->status_code);
  if(message->body != NULL) {
    message->body[message->body_len] = 0;
    printf("the received message (len %lu):\n%s\n\n", message->body_len, message->body);
    //printf("the received message (len %lu):\n", message->body_len);
    //hex_print((unsigned char*) message->body, message->body_len);
  } else {
    puts("no body");
  }
  if(socket->context.requests_used == socket->context.requests_size - 1) {
    TEST_PASS;
    exit(0);
  }
}

int sonnomem(struct https_socket* socket) {
  puts("no mem");
  exit(1);
  return -1;
}

void sonclose(struct https_socket* socket, int reason) {
  printf("socket closed, errno %d, reason %s\n", errno, http_str_close_reason(reason));
}

int main() {
  _debug("Testing http:", 1);
  
  struct http_callbacks http_callbacks;
  http_callbacks.onresponse = onresponse;
  http_callbacks.onnomem = onnomem;
  http_callbacks.onclose = onclose;
  
  struct https_callbacks https_callbacks;
  https_callbacks.onresponse = sonresponse;
  https_callbacks.onnomem = sonnomem;
  https_callbacks.onclose = sonclose;
  
  struct http_options options = {0};
  options.http_callbacks = &http_callbacks;
  options.https_callbacks = &https_callbacks;
  
  options.timeout_after = 5;
  
  struct http_message req = {0};
  req.path = "/c.js";
  req.path_len = strlen(req.path);
  
  struct http_request requests[2] = {0};
  requests[1].request = &req;
  requests[1].no_cache = 1;
  
  options.requests = requests;
  options.requests_len = 2;
  
  int err = http("https://static.diep.io/build_5a2544748e1ac8a5677587d85572be709daf8a66.wasm.js", &options);
  if(err != 0) {
    puts("http() err");
    return 1;
  }
  puts("http() done");
  
  sleep(60);
  return 0;
}