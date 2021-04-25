/*
  Copyright (c) 2021 shädam

  Permission is hereby granted, free of charge, to any person obtaining a copy
  of this software and associated documentation files (the "Software"), to deal
  in the Software without restriction, including without limitation the rights
  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
  copies of the Software, and to permit persons to whom the Software is
  furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included in all
  copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
  SOFTWARE.
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
  _Atomic int state;
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
  struct NETAcceptThreadPool* pool;
  void (*onerror)(struct NETServer*);
  struct NETConnManager* manager;
  int* connections;
  uint32_t conn_count;
#if __WORDSIZE == 64
  uint32_t max_conn_count;
  socklen_t addrlen;
  uint32_t max_connections;
  int flags;
  int _unused2;
#else
  socklen_t addrlen;
  uint32_t max_conn_count;
  int flags;
  uint32_t max_connections;
#endif
  int family;
  int socktype;
  int protocol;
  int sfd;
};

#ifdef __cplusplus
}
#endif

#endif // LScqaaI_Ocw3_3AM4_pv__vxFS3WT_BB