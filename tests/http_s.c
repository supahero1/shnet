#include "tests.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <shnet/http.h>

struct ws_serversock_callbacks callbacks = {0};
struct ws_serversock_callbacks scallbacks = {0};

void onmessage(struct http_serversock* socket, void* _data, uint64_t len) {
  char* data = _data;
  printf("onmessage omfg, len %lu, packet: [", len);
  for(uint64_t i = 0; i < len; ++i) {
    printf("%02x", (unsigned char) data[i]);
    if(i == len - 1) {
      printf("]\n");
    } else {
      printf(", ");
    }
  }
  char* response = "HEWWO!!!";
  printf("ws_send status %d\n", ws_send(socket, (unsigned char*) response, strlen(response), websocket_text, 0));
}

void onclose(struct http_serversock* socket, int code) {
  printf("onclose omfg, code %d\n", code);
}

void onrequest(struct http_server* server, struct http_serversock* socket, struct http_parser_settings* settings, struct http_message* request, struct http_message* response) {
  puts("received a request to http port!");
  socket->callbacks = &callbacks;
  socket->context.settings = server->context.settings;
  const int res = ws(socket, settings, request, response, 1);
  if(res != 1) {
    puts("err res is not 1");
    exit(1);
  }
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
  puts("received a request to https port!");
  
}

int sonnomem(struct https_server* server) {
  puts("no mem");
  exit(1);
  return -1;
}

void sonshutdown(struct https_server* server) {
  puts("server shutdown success");
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
  resources[0].settings = NULL;
  resources[0].http_callback = onrequest;
  //resources[0].https_callback = sonrequest;
  
  server_options.resources = resources;
  server_options.resources_len = 1;
  
  server_options.cert_path = "/path/to/tests/cert.pem";
  server_options.key_path = "/path/to/tests/key.pem";
  server_options.key_type = SSL_FILETYPE_PEM;
  
  server_options.timeout_after = 5;
  
  callbacks.onmessage = onmessage;
  callbacks.onclose = onclose;
  
  int err = http_server("http://0.0.0.0:25566", &server_options);
  if(err != 0) {
    puts("http_server() err");
    return 1;
  }
  puts("http_server() done");
  
  sleep(9999);
  TEST_PASS;
  return 0;
}