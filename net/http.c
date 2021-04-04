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

#include "http.h"

int HTTPv1_1_Parser(uint8_t* buffer, ssize_t len, int flags, struct HTTP_request* request, struct HTTP_parser_session* session) {
  if(len < 18) {
    return HTTP_MALFORMED;
  }
  ssize_t idx;
  if(session != NULL) {
    idx = session->idx;
    switch(session->last_at) {
      case HTTP_PARSE_METHOD: {
        break;
      }
      case HTTP_PARSE_PATH: {
        goto parse_uri;
      }
      case HTTP_PARSE_VERSION: {
        goto parse_version;
      }
      case HTTP_PARSE_HEADERS: {
        goto parse_headers;
      }
      case HTTP_PARSE_BODY: {
        goto parse_body;
      }
    }
  }
  switch(buffer[0]) {
    case 'G': {
      if(memcmp(buffer, &((uint8_t){ 'G', 'E', 'T', ' ' }), 4) != 0) {
        return HTTP_INVAL_METHOD;
      }
      idx = 4;
      request->method = HTTP_GET;
      break;
    }
    case 'H': {
      if(memcmp(buffer + 1, &((uint8_t){ 'E', 'A', 'D', ' ' }), 4) != 0) {
        return HTTP_INVAL_METHOD;
      }
      idx = 5;
      request->method = HTTP_HEAD;
      break;
    }
    case 'P': {
      switch(buffer[1]) {
        case 'O': {
          if(memcmp(buffer + 1, &((uint8_t){ 'O', 'S', 'T', ' ' }), 4) != 0) {
            return HTTP_INVAL_METHOD;
          }
          idx = 5;
          request->method = HTTP_POST;
          break;
        }
        case 'U': {
          if(memcmp(buffer + 1, &((uint8_t){ 'U', 'T', 'S', ' ' }), 4) != 0) {
            return HTTP_INVAL_METHOD;
          }
          idx = 5;
          request->method = HTTP_PUTS;
          break;
        }
        case 'A': {
          if(memcmp(buffer + 2, &((uint8_t){ 'T', 'C', 'H', ' ' }), 4) != 0) {
            return HTTP_INVAL_METHOD;
          }
          idx = 6;
          request->method = HTTP_PATCH;
          break;
        }
        default: {
          return HTTP_MALFORMED;
        }
      }
      break;
    }
    case 'D': {
      if(memcmp(buffer + 1, &((uint8_t){ 'E', 'L', 'E', 'T', 'E', ' ' }), 6) != 0) {
        return HTTP_INVAL_METHOD;
      }
      idx = 7;
      request->method = HTTP_DELETE;
      break;
    }
    case 'T': {
      if(memcmp(buffer + 1, &((uint8_t){ 'R', 'A', 'C', 'E', ' ' }), 5) != 0) {
        return HTTP_INVAL_METHOD;
      }
      idx = 6;
      request->method = HTTP_TRACE;
      break;
    }
    case 'O': {
      if(memcmp(buffer, &((uint8_t){ 'O', 'P', 'T', 'I', 'O', 'N', 'S', ' ' }), 8) != 0) {
        return HTTP_INVAL_METHOD;
      }
      idx = 8;
      request->method = HTTP_OPTIONS;
      break;
    }
    case 'C': {
      if(memcmp(buffer, &((uint8_t){ 'C', 'O', 'N', 'N', 'E', 'C', 'T', ' ' }), 8) != 0) {
        return HTTP_INVAL_METHOD;
      }
      idx = 8;
      request->method = HTTP_CONNECT;
      break;
    }
    default: {
      return HTTP_METHOD_NOTSUP;
    }
  }
  if(session != NULL) {
    session->last_at = HTTP_PARSE_PATH;
  }
  if((flags & HTTP_PARSE_METHOD) != 0) {
    return 0;
  }
  parse_uri:;
  uint8_t* end = memchr(buffer + idx, ' ', len - idx);
  if(end == NULL) {
    return HTTP_MALFORMED;
  }
  if(idx + length  > len - 13) {
    return HTTP_MALFORMED;
  }
  if(session != NULL) {
    session->last_at = HTTP_PARSE_VERSION;
  }
  if((flags & HTTP_PARSE_PATH) != 0) {
    return 0;
  }
  idx += length;
  parse_version:
  if(memcmp(buffer + idx, &((uint8_t){ ' ', 'H', 'T', 'T', 'P', '/', '1', '.', '1', '\\', 'r', '\\', 'n' }), 13) != 0) {
    if(buffer[idx + 8] == '0') {
      return HTTP_VERSION_NOTSUP;
    } else {
      return HTTP_MALFORMED;
    }
  }
  if(session != NULL) {
    session->last_at = HTTP_PARSE_HEADERS;
  }
  if((flags & HTTP_PARSE_VERSION) != 0) {
    return 0;
  }
  idx += 6;
  parse_headers:
  while(memcmp(buffer + idx, &((uint8_t){ '\\', 'r', '\\', 'n' }), 4) != 0) {
    
  }
  parse_body:
  request->body = buffer + idx;
  request->body_length = len - idx;
  return 0;
}