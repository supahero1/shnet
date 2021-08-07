#include "tests.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <shnet/http.h>

struct ws_serversock_callbacks callbacks = {0};
struct wss_serversock_callbacks scallbacks = {0};

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
  hex_print((unsigned char*) data, len);
  char* response = "HEWWO!!!";
  printf("ws_send status %d\n", ws_send(socket, (unsigned char*) response, strlen(response), websocket_text, ws_dont_free));
}

void onclose(struct http_serversock* socket, int code) {
  printf("onclose omfg, code %d\n", code);
}

void ws_onrequest(struct http_server* server, struct http_serversock* socket, struct http_message* request, struct http_message* response) {
  puts("received a request to websocket port!");
  socket->callbacks = &callbacks;
  socket->context.settings = server->context.default_settings;
  socket->context.close_onreadclose = 1;
  const int res = ws(socket, request, response, 1);
  if(res != 1) {
    puts("err res is not 1");
    exit(1);
  }
}

void onrequest(struct http_server* server, struct http_serversock* socket, struct http_message* request, struct http_message* response) {
  puts("received a request to http port!");
  response->status_code = http_s_ok;
  response->reason_phrase = http1_default_reason_phrase(response->status_code);
  response->reason_phrase_len = http1_default_reason_phrase_len(response->status_code);
  response->body = "hi!!!";
  response->body_len = strlen(response->body);
  response->headers[response->headers_len++] = (struct http_header) { "Content-Type", "text/html", 9, 12, 0, 0 };
}

int onnomem(struct http_server* server) {
  puts("no mem");
  exit(1);
  return -1;
}

void onshutdown(struct http_server* server) {
  puts("server shutdown success");
}

void sonmessage(struct https_serversock* socket, void* _data, uint64_t len) {
  char* data = _data;
  printf("sonmessage omfg, len %lu, packet: [", len);
  for(uint64_t i = 0; i < len; ++i) {
    printf("%02x", (unsigned char) data[i]);
    if(i == len - 1) {
      printf("]\n");
    } else {
      printf(", ");
    }
  }
  hex_print((unsigned char*) data, len);
  char* response = "HEWWO!!!";
  printf("ws_send status %d\n", ws_send(socket, (unsigned char*) response, strlen(response), websocket_text, ws_dont_free));
}

void sonclose(struct https_serversock* socket, int code) {
  printf("onclose omfg, code %d\n", code);
}

void ws_sonrequest(struct https_server* server, struct https_serversock* socket, struct http_message* request, struct http_message* response) {
  puts("received a request to secure websocket port!");
  socket->callbacks = &scallbacks;
  socket->context.settings = server->context.default_settings;
  socket->context.close_onreadclose = 1;
  const int res = ws(socket, request, response, 1);
  if(res != 1) {
    puts("err res is not 1");
    exit(1);
  }
}

void sonrequest(struct https_server* server, struct https_serversock* socket, struct http_message* request, struct http_message* response) {
  puts("received a request to https port!");
  response->status_code = http_s_ok;
  response->reason_phrase = http1_default_reason_phrase(response->status_code);
  response->reason_phrase_len = http1_default_reason_phrase_len(response->status_code);
  response->body = "hi!!!";
  response->body_len = strlen(response->body);
  response->headers[response->headers_len++] = (struct http_header) { "Content-Type", "text/html", 9, 12, 0, 0 };
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
  
  struct http_server_callbacks server_http_callbacks = {0};
  server_http_callbacks.onnomem = onnomem;
  server_http_callbacks.onshutdown = onshutdown;
  
  struct https_server_callbacks server_https_callbacks = {0};
  server_https_callbacks.onnomem = sonnomem;
  server_https_callbacks.onshutdown = sonshutdown;
  
  struct http_server_options server_options = {0};
  server_options.http_callbacks = &server_http_callbacks;
  server_options.https_callbacks = &server_https_callbacks;
  
  struct http_resource resources[] = { { "/lmfao", NULL, { .https_callback = sonrequest } }, { "/ws", NULL, { .https_callback = ws_sonrequest } } };
  server_options.resources = resources;
  server_options.resources_len = sizeof(resources) / sizeof(struct http_resource);
  
  server_options.ctx = tls_ctx("./tests/cert.pem", "./tests/key.pem", NULL, tls_rsa_key);
  
  server_options.timeout_after = 5;
  
  callbacks.onmessage = onmessage;
  callbacks.onclose = onclose;
  
  scallbacks.onmessage = sonmessage;
  scallbacks.onclose = sonclose;
  
  int err = http_server("https://0.0.0.0:5000", &server_options);
  if(err != 0) {
    puts("http_server() err");
    return 1;
  }
  puts("http_server() done");
  
  sleep(9999);
  TEST_PASS;
  return 0;
}