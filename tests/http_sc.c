#include "tests.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <shnet/http.h>

void onrequest(struct http_server* server, struct http_serversock* socket, struct http_message* request, struct http_message* response) {
  puts("received a request, cool!");
  response->status_code = 200;
  response->reason_phrase = http1_default_reason_phrase(response->status_code);
  response->reason_phrase_len = http1_default_reason_phrase_len(response->status_code);
  response->body = "hi!!!";
  response->body_len = 5;
}

int onnomem(struct http_server* server) {
  puts("no mem");
  exit(1);
  return -1;
}

void onshutdown(struct http_server* server) {
  puts("server shutdown success");
}

void sonrequest(struct https_server* server, struct https_serversock* socket, struct http_parser_settings* settings, struct http_message* request, struct http_message* response) {
  puts("received a request, cool!");
  response->status_code = 200;
  response->reason_phrase = http1_default_reason_phrase(response->status_code);
  response->reason_phrase_len = http1_default_reason_phrase_len(response->status_code);
  response->body = "hi!!!";
  response->body_len = 5;
}

int sonnomem(struct https_server* server) {
  puts("no mem");
  exit(1);
  return -1;
}

void sonshutdown(struct https_server* server) {
  puts("server shutdown success");
}




void sock_onresponse(struct http_socket* socket, struct http_message* message) {
  printf("got a response to request nr %u, status code %u\n", socket->context.requests_used, message->status_code);
  if(message->body != NULL) {
    printf("the received message (len %lu):\n%s\n\n", message->body_len, message->body);
  }
}

int sock_onnomem(struct http_socket* socket) {
  puts("no mem");
  exit(1);
  return -1;
}

void sock_onclose(struct http_socket* socket, int reason) {
  printf("socket closed, errno %d, reason %s\n", errno, http_str_close_reason(reason));
}

void sock_sonresponse(struct https_socket* socket, struct http_message* message) {
  printf("got a response to request nr %u, status code %u\n", socket->context.requests_used, message->status_code);
  if(message->body != NULL) {
    printf("the received message (len %lu):\n%s\n\n", message->body_len, message->body);
  }
}

int sock_sonnomem(struct https_socket* socket) {
  puts("no mem");
  exit(1);
  return -1;
}

void sock_sonclose(struct https_socket* socket, int reason) {
  printf("socket closed, errno %d, reason %s\n", errno, http_str_close_reason(reason));
}

int main() {
  _debug("Testing http_s:", 1);
  
  struct http_server_callbacks server_http_callbacks;
  server_http_callbacks.onnomem = onnomem;
  server_http_callbacks.onshutdown = onshutdown;
  
  struct https_server_callbacks server_https_callbacks;
  server_https_callbacks.onnomem = sonnomem;
  server_https_callbacks.onshutdown = sonshutdown;
  
  struct http_server_options server_options = {0};
  server_options.http_callbacks = &server_http_callbacks;
  server_options.https_callbacks = &server_https_callbacks;
  
  struct http_resource resources[1];
  resources[0].path = "/";
  //resources[0].http_callback = onrequest;
  resources[0].https_callback = sonrequest;
  
  server_options.resources = resources;
  server_options.resources_len = 1;
  
  server_options.cert_path = "/path/to/tests/cert.pem";
  server_options.key_path = "/path/to/tests/key.pem";
  server_options.key_type = SSL_FILETYPE_PEM;
  
  server_options.timeout_after = 5;
  
  int err = http_server("https://localhost:2531", &server_options);
  if(err != 0) {
    puts("http_server() err");  
    return 1;
  }
  puts("http_server() done");
  
  
  
  struct http_callbacks sock_http_callbacks;
  sock_http_callbacks.onresponse = sock_onresponse;
  sock_http_callbacks.onnomem = sock_onnomem;
  sock_http_callbacks.onclose = sock_onclose;
  
  struct https_callbacks sock_https_callbacks;
  sock_https_callbacks.onresponse = sock_sonresponse;
  sock_https_callbacks.onnomem = sock_sonnomem;
  sock_https_callbacks.onclose = sock_sonclose;
  
  struct http_options sock_options = {0};
  sock_options.http_callbacks = &sock_http_callbacks;
  sock_options.https_callbacks = &sock_https_callbacks;
  
  sock_options.timeout_after = 5;
  
  struct http_message req = {0};
  req.path = "/d.js";
  req.path_len = strlen(req.path);
  
  struct http_request requests[2] = {0};
  requests[1].request = &req;
  
  sock_options.requests = requests;
  sock_options.requests_len = 1;
  
  err = http("https://localhost:2531/", &sock_options);
  if(err != 0) {
    puts("http() err");
    return 1;
  }
  puts("http() done");
  
  
  sleep(1);
  https_server_shutdown(server_options.https_server);
  sleep(1);
  TEST_PASS;
  return 0;
}