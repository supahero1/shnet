/*
   Copyright 2021 shädam

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at

       http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License.
*/

#ifndef LScqaaI_Ocw3_3AM4_pv__vxFS3WT_BB
#define LScqaaI_Ocw3_3AM4_pv__vxFS3WT_BB 1

#ifdef __cplusplus
extern "C" {
#endif

#include <netdb.h>

enum NETFlags { NET_OPEN, NET_SEND_CLOSING, NET_RECEIVE_CLOSING, NET_CLOSED };

struct NETSocket {
  struct sockaddr addr;
  void (*onmessage)(struct NETSocket*);
  void (*onclose)(struct NETSocket*);
  void (*onerror)(struct NETSocket*);
  void (*onsent)(struct NETSocket*);
  void* send_buffer;
  ssize_t send_length;
  socklen_t addrlen;
  int state;
  int flags;
  int server;
  int family;
  int socktype;
  int protocol;
  int sfd;
};

struct NETServer {
  struct sockaddr addr;
  void (*onconnection)(struct NETServer*, struct NETSocket);
  struct NETServerThreadPool* pool;
  void (*onerror)(struct NETServer*);
  struct NETConnManager* manager;
  int* connections;
  uint32_t conn_count;
  uint32_t max_conn_count;
  socklen_t addrlen;
  uint16_t handshake_timeout;
  uint16_t http_request_timeout;
  int flags;
  int family;
  int socktype;
  int protocol;
  int sfd;
};

#ifdef __cplusplus
}
#endif

#endif // LScqaaI_Ocw3_3AM4_pv__vxFS3WT_BB