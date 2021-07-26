#include "tests.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <shnet/http.h>

void onresponse(struct http_socket* socket, struct http_message* message) {
  printf("got a response to request nr %u, status code %u\n", socket->context.requests_used, message->status_code);
  if(message->body != NULL) {
    message->body = realloc(message->body, message->body_len + 1);
    if(message->body == NULL) {
      TEST_FAIL;
    }
    message->body[message->body_len] = 0;
    printf("the received message (len %lu):\n%s\n\n", message->body_len, message->body);
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
    message->body = realloc(message->body, message->body_len + 1);
    if(message->body == NULL) {
      TEST_FAIL;
    }
    message->body[message->body_len] = 0;
    printf("the received message (len %lu):\n%s\n\n", message->body_len, message->body);
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
  req.path = "/d.js";
  req.path_len = strlen(req.path);
  
  struct http_request requests[2] = {0};
  requests[1].request = &req;
  
  options.requests = requests;
  options.requests_len = 2;
  
  int err = http("https://static.diep.io/build_7d8655edcf09098d26ec676963ac899405b72f5d.wasm.js", &options);
  if(err != 0) {
    puts("http() err");
    return 1;
  }
  puts("http() done");
  
  sleep(2);
  TEST_PASS;
  return 0;
}