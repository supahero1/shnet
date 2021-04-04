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

#ifndef MyxpZpo4_G3__8_gF__x___zpu_W5ukt
#define MyxpZpo4_G3__8_gF__x___zpu_W5ukt 1

#ifdef __cplusplus
extern "C" {
#endif

#include "net.h"

enum HTTPCodes {
  HTTP_GET,
  HTTP_HEAD,
  HTTP_POST,
  HTTP_PUT,
  HTTP_DELETE,
  HTTP_TRACE,
  HTTP_OPTIONS,
  HTTP_CONNECT,
  HTTP_PATCH,
  
  HTTP_PARSE_METHOD = 0,
  HTTP_PARSE_PATH,
  HTTP_PARSE_VERSION,
  HTTP_PARSE_HEADERS,
  HTTP_PARSE_BODY,
  
  HTTP_METHOD_NOTSUP = 1,
  HTTP_INVAL_METHOD,
  HTTP_MALFORMED,
  HTTP_VERSION_NOTSUP
};

struct HTTP_request {
  char* path;
  struct HTTP_header* headers;
  uint8_t* body;
  uint32_t path_length;
  uint32_t header_count;
  uint32_t body_length;
  int method;
};

struct HTTP_parser_session {
  ssize_t idx;
  int last_at;
};

int HTTPv1_1_raw_parser(uint8_t*, ssize_t, int, struct HTTP_request*, struct HTTP_settings*, struct HTTP_parser_session*);




#ifdef __cplusplus
}
#endif

#endif // MyxpZpo4_G3__8_gF__x___zpu_W5ukt