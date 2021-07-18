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

void sonrequest(struct https_server* server, struct https_serversock* socket, struct http_message* request, struct http_message* response) {
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
  resources[0].http_callback = onrequest;
  //resources[0].https_callback = sonrequest;
  
  server_options.resources = resources;
  server_options.resources_len = 1;
  
  /*server_options.cert_path = "/path/to/tests/cert.pem";
  server_options.cert_type = SSL_FILETYPE_PEM;
  server_options.key_path = "/path/to/tests/key.pem";
  server_options.key_type = SSL_FILETYPE_PEM;*/
  
  server_options.timeout_after = 5;
  
  int err = http_server("http://127.0.0.1:25566", &server_options);
  if(err != 0) {
    puts("http_server() err");  
    return 1;
  }
  puts("http_server() done");
  
  sleep(9999);
  TEST_PASS;
  return 0;
}