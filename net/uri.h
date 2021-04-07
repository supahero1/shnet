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

#ifndef IcI__bQ3o__ci4B_f0q_RdU1_jBTxv4_
#define IcI__bQ3o__ci4B_f0q_RdU1_jBTxv4_ 1

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

enum URI_codes {
  URI_PARSE_SCHEME = 1,
  URI_PARSE_AUTHORITY,
  URI_PARSE_USERINFO = 2,
  URI_PARSE_HOST = 4,
  URI_PARSE_PORT = 8,
  URI_PARSE_PATH = 16,
  URI_PARSE_QUERY = 32,
  URI_PARSE_FRAGMENT = 64,
  URI_NO_AUTHORITY = 128,
  URI_NO_USERINFO = 128,
  URI_NO_PORT = 256,
  URI_NO_PATH = 512,
  URI_NO_QUERY = 1024,
  URI_NO_FRAGMENT = 2048,
  URI_STRICT = 4096,
  
  URI_VALID = 0,
  URI_MALFORMED,
  URI_NOT_ALLOWED,
  URI_SCHEME_TOO_LONG,
  URI_USERINFO_TOO_LONG,
  URI_HOST_TOO_LONG
  URI_PATH_TOO_LONG,
  URI_QUERY_TOO_LONG,
  URI_FRAGMENT_TOO_LONG
};

struct URI {
  char* scheme;
  char* userinfo;
  char* host;
  char* path;
  char* query;
  char* fragment;
  uint32_t scheme_length;
  uint32_t userinfo_length;
  uint32_t host_length;
  uint32_t path_length;
  uint32_t query_length;
  uint32_t fragment_length;
  uint16_t port;
};

struct URI_settings {
  uint32_t max_scheme_length;
  uint32_t max_userinfo_length;
  uint32_t max_host_length;
  uint32_t max_path_length;
  uint32_t max_query_length;
  uint32_t max_fragment_length;
};

struct URI_session {
  uint32_t idx;
  int last_at;
};

extern int URI_parser(void*, const uin32_t, const int, struct URI* const, const struct URI_settings* const, struct URI_session* const);

#ifdef __cplusplus
}
#endif

#endif // IcI__bQ3o__ci4B_f0q_RdU1_jBTxv4_