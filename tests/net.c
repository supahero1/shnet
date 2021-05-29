#include "tests.h"

#include <math.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <shnet/net.h>
#include <shnet/time.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

int main() {
  printf_debug("Testing net.c:", 1);
  char ip[41];
  struct addrinfo hints = net_get_addr_struct(any_family, any_socktype, any_protocol, numeric_service);
  struct addrinfo* res = net_get_address("google.com", "80", &hints);
  if(res == NULL) {
    printf("err %s\n", net_get_address_strerror(errno));
    TEST_FAIL;
  }
  struct addrinfo* info = res;
  do {
    int err = net_address_to_string(info->ai_addr, ip);
    if(err == net_failure) {
      TEST_FAIL;
    }
    err = net_string_to_address(info->ai_addr, ip);
    if(err == net_failure) {
      TEST_FAIL;
    }
    err = net_address_to_string(info->ai_addr, ip);
    if(err == net_failure) {
      TEST_FAIL;
    }
    err = net_get_name(info->ai_addr, net_get_addrlen(info->ai_addr), ip, sizeof(ip), 0);
    if(err != 0) {
      if(err != EAI_NONAME) {
        printf("err %s\n", net_get_address_strerror(err));
        TEST_FAIL;
      }
    }
    info = info->ai_next;
  } while(info != NULL);
  net_get_address_free(res);
  info = NULL;
  res = NULL;
  TEST_PASS;
  return 0;
}