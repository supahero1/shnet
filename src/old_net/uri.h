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

#ifndef IcI__bQ3o__ci4B_f0q_RdU1_jBTxv4_
#define IcI__bQ3o__ci4B_f0q_RdU1_jBTxv4_ 1

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

enum URI_codes {
  URI_PARSE_SCHEME = 1,
  URI_PARSE_USERINFO,
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