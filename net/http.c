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

#include <errno.h>
#include <string.h>
#include <stdlib.h>

#include <stdio.h>

struct HTTP_settings HTTP_default_settings() {
  return (struct HTTP_settings) {
    .max_path_length = 4096,
    .max_header_amount = 64,
    .max_header_name_length = 64,
    .max_header_value_length = 4096,
    .request_idle_timeout = 8000,
    .max_reason_phrase_length = 64
  };
}

static const uint8_t HTTP_tokens[] = {
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 1, 0, 1, 1, 1, 1, 1, 0, 0, 1, 1, 0, 1, 1, 0,
  1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0,
  0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
  1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 1, 1,
  1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,
  1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 1, 0, 1, 0,
  
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

int HTTPv1_1_request_parser(uint8_t* const buffer, const uint32_t len, const int flags, struct HTTP_request* const request, const struct HTTP_settings* const settings, struct HTTP_parser_session* const session) {
  if(len < 18) { // GET / HTTP/1.1\r\n\r\n
    return HTTP_MALFORMED;
  }
  uint32_t idx;
  if(session != NULL) {
    idx = session->idx;
    switch(session->last_at) {
      case HTTP_PARSE_METHOD: {
        break;
      }
      case HTTP_PARSE_PATH: {
        goto parse_path;
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
    session->last_at = HTTP_PARSE_METHOD;
  }
  *request = (struct HTTP_request) {
    .path = NULL,
    .headers = NULL,
    .body = NULL,
    .path_length = 0,
    .header_amount = 0,
    .method = 0
  };
  switch(buffer[0]) {
    case 'G': {
      if(memcmp(buffer, &((uint8_t[]){ 'G', 'E', 'T', ' ' }), 4) != 0) {
        return HTTP_INVAL_METHOD;
      }
      idx = 4;
      request->method = HTTP_GET;
      break;
    }
    case 'H': {
      if(memcmp(buffer + 1, &((uint8_t[]){ 'E', 'A', 'D', ' ' }), 4) != 0) {
        return HTTP_INVAL_METHOD;
      }
      idx = 5;
      request->method = HTTP_HEAD;
      break;
    }
    case 'P': {
      switch(buffer[1]) {
        case 'O': {
          if(memcmp(buffer + 1, &((uint8_t[]){ 'O', 'S', 'T', ' ' }), 4) != 0) {
            return HTTP_INVAL_METHOD;
          }
          idx = 5;
          request->method = HTTP_POST;
          break;
        }
        case 'U': {
          if(memcmp(buffer, &((uint8_t[]){ 'P', 'U', 'T', ' ' }), 4) != 0) {
            return HTTP_INVAL_METHOD;
          }
          idx = 4;
          request->method = HTTP_PUT;
          break;
        }
        case 'A': {
          if(memcmp(buffer + 2, &((uint8_t[]){ 'T', 'C', 'H', ' ' }), 4) != 0) {
            return HTTP_INVAL_METHOD;
          }
          idx = 6;
          request->method = HTTP_PATCH;
          break;
        }
        default: {
          return HTTP_INVAL_METHOD;
        }
      }
      break;
    }
    case 'D': {
      if(memcmp(buffer + 1, &((uint8_t[]){ 'E', 'L', 'E', 'T', 'E', ' ' }), 6) != 0) {
        return HTTP_INVAL_METHOD;
      }
      idx = 7;
      request->method = HTTP_DELETE;
      break;
    }
    case 'T': {
      if(memcmp(buffer + 1, &((uint8_t[]){ 'R', 'A', 'C', 'E', ' ' }), 5) != 0) {
        return HTTP_INVAL_METHOD;
      }
      idx = 6;
      request->method = HTTP_TRACE;
      break;
    }
    case 'O': {
      if(memcmp(buffer, &((uint8_t[]){ 'O', 'P', 'T', 'I', 'O', 'N', 'S', ' ' }), 8) != 0) {
        return HTTP_INVAL_METHOD;
      }
      idx = 8;
      request->method = HTTP_OPTIONS;
      break;
    }
    case 'C': {
      if(memcmp(buffer, &((uint8_t[]){ 'C', 'O', 'N', 'N', 'E', 'C', 'T', ' ' }), 8) != 0) {
        return HTTP_INVAL_METHOD;
      }
      idx = 8;
      request->method = HTTP_CONNECT;
      break;
    }
    default: {
      return HTTP_INVAL_METHOD;
    }
  }
  puts("parsed method");
  if(session != NULL) {
    session->last_at = HTTP_PARSE_PATH;
    session->idx = idx;
  }
  if((flags & HTTP_PARSE_METHOD) != 0) {
    return HTTP_VALID;
  }
  parse_path:;
  uint8_t* end = memchr(buffer + idx, ' ', len - idx);
  if(end == NULL) {
    return HTTP_MALFORMED;
  }
  uint32_t length = (uint32_t)((uintptr_t) end - (uintptr_t)(buffer + idx));
  if(length > settings->max_path_length) {
    errno = HTTP_PATH_TOO_LONG;
    return HTTP_NOT_ALLOWED;
  }
  if(idx + length  > len - 13) {
    return HTTP_MALFORMED;
  }
  request->path = (char*) buffer + idx;
  request->path_length = length;
  puts("parsed path");
  if(session != NULL) {
    session->last_at = HTTP_PARSE_VERSION;
    session->idx = idx;
  }
  if((flags & HTTP_PARSE_PATH) != 0) {
    return HTTP_VALID;
  }
  idx += length;
  parse_version:
  if(memcmp(buffer + idx, &((uint8_t[]){ ' ', 'H', 'T', 'T', 'P', '/', '1', '.', '1', '\r', '\n' }), 11) != 0) {
    if(memcmp(buffer + idx, &((uint8_t[]){ ' ', 'H', 'T', 'T', 'P', '/', '1', '.', '0', '\r', '\n' }), 11) == 0) {
      return HTTP_VERSION_NOTSUP;
    } else {
      return HTTP_MALFORMED;
    }
  }
  idx += 11;
  puts("parsed version");
  if(session != NULL) {
    session->last_at = HTTP_PARSE_HEADERS;
    session->idx = idx;
  }
  if((flags & HTTP_PARSE_VERSION) != 0) {
    return HTTP_VALID;
  }
  parse_headers:
  request->headers = malloc(sizeof(struct HTTP_header) * settings->max_header_amount);
  if(request->headers == NULL) {
    return HTTP_NOMEM;
  }
  while(memcmp(buffer + idx, &((uint8_t[]){ '\r', '\n' }), 2) != 0) {
    uint32_t i = idx;
    while(1) {
      if(len - i < 6) {
        goto free_malformed;
      }
      if(buffer[i] == ':') {
        if(i == idx || ((flags & HTTP_ALLOW_REPETITIVE_HEADER_WHITESPACE) == 0 && (buffer[i + 1] == ' ' || buffer[i + 1] == '\t') && (buffer[i + 2] == ' ' || buffer[i + 2] == '\t'))) {
          goto free_malformed;
        }
        request->headers[request->header_amount] = (struct HTTP_header) {
          .name = (char*) buffer + idx,
          .value = NULL,
          .name_length = i - idx,
          .value_length = 0
        };
        printf("found a header name, length %u\n", i - idx);
        idx = i + 1;
        break;
      } else if(HTTP_tokens[buffer[i]] == 0) {
        goto free_malformed;
      }
      ++i;
    }
    while(1) {
      if(idx == len - 1) {
        goto free_malformed;
      }
      if(buffer[idx] == '\r' && idx != len - 2 && buffer[idx + 1] == '\n') {
        free(request->headers);
        return HTTP_ILLEGAL_SPACE;
      } else if(buffer[idx] != ' ' && buffer[idx] != '\t') {
        break;
      } else {
        ++idx;
      }
    }
    i = idx;
    while(1) {
      if(len - i < 4) {
        goto free_malformed;
      }
      if(buffer[i] == '\r') {
        if(buffer[i + 1] != '\n') {
          goto free_malformed;
        } else {
          request->headers[request->header_amount].value = (char*) buffer + idx;
          request->headers[request->header_amount++].value_length = i - idx;
          printf("found a header value, length %u\n", i - idx);
          idx = i + 2;
          break;
        }
      } else if(HTTP_tokens[buffer[i]] == 0 && buffer[i] != ' ' && buffer[i] != '\t') {
        goto free_malformed;
      }
      ++i;
    }
    if(request->header_amount == settings->max_header_amount && idx < len && buffer[idx] != '\r') {
      free(request->headers);
      errno = HTTP_TOO_MANY_HEADERS;
      return HTTP_NOT_ALLOWED;
    }
  }
  request->headers = realloc(request->headers, sizeof(struct HTTP_header) * request->header_amount);
  idx += 2;
  puts("parsed headers");
  if(session != NULL) {
    session->last_at = HTTP_PARSE_BODY;
    session->idx = idx;
  }
  if((flags & HTTP_PARSE_HEADERS) != 0) {
    return HTTP_VALID;
  }
  parse_body:
  request->body = buffer + idx;
  return HTTP_VALID;
  free_malformed:
  free(request->headers);
  return HTTP_MALFORMED;
}

int HTTPv1_1_response_parser(uint8_t* const buffer, const uint32_t len, const int flags, struct HTTP_response* const response, const struct HTTP_settings* const settings, struct HTTP_parser_session* const session) {
  if(len < 17) { // HTTP/1.1 100 \r\n\r\n
    return HTTP_MALFORMED;
  }
  uint32_t idx;
  if(session != NULL) {
    idx = session->idx;
    switch(session->last_at) {
      case HTTP_PARSE_STATUS: {
        break;
      }
      case HTTP_PARSE_REASON_PHRASE: {
        goto parse_reason_phrase;
      }
      case HTTP_PARSE_HEADERS: {
        goto parse_headers;
      }
      case HTTP_PARSE_BODY: {
        goto parse_body;
      }
    }
    session->last_at = HTTP_PARSE_STATUS;
  } else {
    idx = 0;
  }
  *response = (struct HTTP_response) {
    .headers = NULL,
    .body = NULL,
    .reason_phrase = NULL,
    .header_amount = 0,
    .status_code = 0,
    .reason_phrase_length = 0
  };
  if(memcmp(buffer, &((uint8_t[]){ 'H', 'T', 'T', 'P', '/', '1', '.', '1', ' ' }), 9) != 0) {
    if(memcmp(buffer, &((uint8_t[]){ 'H', 'T', 'T', 'P', '/', '1', '.', '0', ' ' }), 9) == 0) {
      return HTTP_VERSION_NOTSUP;
    } else {
      return HTTP_MALFORMED;
    }
  }
  idx += 9;
  if(buffer[idx    ] < 49 || buffer[idx    ] > 57) {
    return HTTP_MALFORMED;
  }
  if(buffer[idx + 1] < 48 || buffer[idx + 1] > 57) {
    return HTTP_MALFORMED;
  }
  if(buffer[idx + 2] < 48 || buffer[idx + 2] > 57) {
    return HTTP_MALFORMED;
  }
  if(buffer[idx + 3] != ' ') {
    return HTTP_MALFORMED;
  }
  uint32_t code = ((buffer[idx + 0] - 48) * 100) + ((buffer[idx + 1] - 48) * 10) + (buffer[idx + 2] - 48);
  switch(code) {
    case HTTP_CONTINUE:
    case HTTP_SWITCHING_PROTOCOLS:
    case HTTP_PROCESSING:
    case HTTP_OK:
    case HTTP_CREATED:
    case HTTP_ACCEPTED:
    case HTTP_NON_AUTHORITATIVE_INFORMATION:
    case HTTP_NO_CONTENT:
    case HTTP_RESET_CONTENT:
    case HTTP_PARTIAL_CONTENT:
    case HTTP_MULTI_STATUS:
    case HTTP_ALREADY_REPORTED:
    case HTTP_IM_USED:
    case HTTP_MULTIPLE_CHOICES:
    case HTTP_MOVED_PERMANENTLY:
    case HTTP_FOUND:
    case HTTP_SEE_OTHER:
    case HTTP_NOT_MODIFIED:
    case HTTP_USE_PROXY:
    case HTTP_TEMPORARY_REDIRECT:
    case HTTP_PERMANENT_REDIRECT:
    case HTTP_BAD_REQUEST:
    case HTTP_UNAUTHORIZED:
    case HTTP_PAYMENT_REQUIRED:
    case HTTP_FORBIDDEN:
    case HTTP_NOT_FOUND:
    case HTTP_METHOD_NOT_ALLOWED:
    case HTTP_NOT_ACCEPTABLE:
    case HTTP_PROXY_AUTHENTICATION_REQUIRED:
    case HTTP_REQUEST_TIMEOUT:
    case HTTP_CONFLICT:
    case HTTP_GONE:
    case HTTP_LENGTH_REQUIRED:
    case HTTP_PRECONDITION_FAILED:
    case HTTP_PAYLOAD_TOO_LARGE:
    case HTTP_REQUEST_URI_TOO_LONG:
    case HTTP_UNSUPPORTED_MEDIA_TYPE:
    case HTTP_REQUESTED_RANGE_NOT_SATISFIABLE:
    case HTTP_EXPECTATION_FAILED:
    case HTTP_IM_A_TEAPOT:
    case HTTP_MISDIRECTED_REQUEST:
    case HTTP_UNPROCESSABLE_ENTITY:
    case HTTP_LOCKED:
    case HTTP_FAILED_DEPENDENCY:
    case HTTP_UPGRADE_REQUIRED:
    case HTTP_PRECONDITION_REQUIRED:
    case HTTP_TOO_MANY_REQUESTS:
    case HTTP_REQUEST_HEADER_FIELDS_TOO_LARGE:
    case HTTP_CONNECTION_CLOSED_WITHOUT_RESPONSE:
    case HTTP_UNAVAILABLE_FOR_LEGAL_REASONS:
    case HTTP_CLIENT_CLOSED_REQUEST:
    case HTTP_INTERNAL_SERVER_ERROR:
    case HTTP_NOT_IMPLEMENTED:
    case HTTP_BAD_GATEWAY:
    case HTTP_SERVICE_UNAVAILABLE:
    case HTTP_GATEWAY_TIMEOUT:
    case HTTP_HTTP_VERSION_NOT_SUPPORTED:
    case HTTP_VARIANT_ALSO_NEGOTIATES:
    case HTTP_INSUFFICIENT_STORAGE:
    case HTTP_LOOP_DETECTED:
    case HTTP_NOT_EXTENDED:
    case HTTP_NETWORK_AUTHENTICATION_REQUIRED:
    case HTTP_NETWORK_CONNECT_TIMEOUT_ERROR: {
      break;
    }
    default: {
      return HTTP_INVAL_STATUS_CODE;
    }
  }
  puts("parsed status code");
  response->status_code = code;
  idx += 4;
  if((flags & HTTP_IGNORE_REASON_PHRASE) == 0) {
    parse_reason_phrase:
    for(uint32_t i = 0; i < settings->max_reason_phrase_length; ++i) {
      if(len - idx - i < 4) {
        return HTTP_MALFORMED;
      }
      if(buffer[idx + i] == '\r') { // assuming that before calling this function we calculated `len` by finding \r\n\r\n
        if(buffer[idx + i + 1] != '\n') {
          return HTTP_MALFORMED;
        } else {
          response->reason_phrase = (char*) buffer + idx;
          response->reason_phrase_length = i;
          idx += i + 2;
          break;
        }
      } else if(buffer[idx + i] == '\n') {
        return HTTP_MALFORMED;
      } else if(i == settings->max_reason_phrase_length - 1) {
        return HTTP_REASON_PHRASE_TOO_LONG;
      }
    }
    puts("parsed reason phrase");
  }
  if(session != NULL) {
    session->last_at = HTTP_PARSE_HEADERS;
    session->idx = idx;
  }
  if((flags & HTTP_PARSE_STATUS) != 0) {
    return HTTP_VALID;
  }
  parse_headers:
  response->headers = malloc(sizeof(struct HTTP_header) * settings->max_header_amount);
  if(response->headers == NULL) {
    return HTTP_NOMEM;
  }
  while(memcmp(buffer + idx, &((uint8_t[]){ '\r', '\n' }), 2) != 0) {
    uint32_t i = idx;
    while(1) {
      if(len - i < 6) {
        goto free_malformed;
      }
      if(buffer[i] == ':') {
        if(i == idx || ((flags & HTTP_ALLOW_REPETITIVE_HEADER_WHITESPACE) == 0 && (buffer[i + 1] == ' ' || buffer[i + 1] == '\t') && (buffer[i + 2] == ' ' || buffer[i + 2] == '\t'))) {
          goto free_malformed;
        }
        response->headers[response->header_amount] = (struct HTTP_header) {
          .name = (char*) buffer + idx,
          .value = NULL,
          .name_length = i - idx,
          .value_length = 0
        };
        printf("found a header name, length %u\n", i - idx);
        idx = i + 1;
        break;
      } else if(HTTP_tokens[buffer[i]] == 0) {
        goto free_malformed;
      }
      ++i;
    }
    while(1) {
      if(idx == len - 1) {
        goto free_malformed;
      }
      if(buffer[idx] == '\r' && idx != len - 2 && buffer[idx + 1] == '\n') {
        free(response->headers);
        return HTTP_ILLEGAL_SPACE;
      } else if(buffer[idx] != ' ' && buffer[idx] != '\t') {
        break;
      } else {
        ++idx;
      }
    }
    i = idx;
    while(1) {
      if(len - i < 4) {
        goto free_malformed;
      }
      if(buffer[i] == '\r') {
        if(buffer[i + 1] != '\n') {
          goto free_malformed;
        } else {
          response->headers[response->header_amount].value = (char*) buffer + idx;
          response->headers[response->header_amount++].value_length = i - idx;
          printf("found a header value, length %u\n", i - idx);
          idx = i + 2;
          break;
        }
      } else if(HTTP_tokens[buffer[i]] == 0 && buffer[i] != ' ' && buffer[i] != '\t') {
        goto free_malformed;
      }
      ++i;
    }
    if(response->header_amount == settings->max_header_amount && idx < len && buffer[idx] != '\r') {
      free(response->headers);
      errno = HTTP_TOO_MANY_HEADERS;
      return HTTP_NOT_ALLOWED;
    }
  }
  response->headers = realloc(response->headers, sizeof(struct HTTP_header) * response->header_amount);
  idx += 2;
  puts("parsed headers");
  if(session != NULL) {
    session->last_at = HTTP_PARSE_BODY;
    session->idx = idx;
  }
  if((flags & HTTP_PARSE_HEADERS) != 0) {
    return HTTP_VALID;
  }
  parse_body:
  response->body = buffer + idx;
  return HTTP_VALID;
  free_malformed:
  free(response->headers);
  return HTTP_MALFORMED;
}

char* HTTP_strerror(const int err) {
  switch(err) {
    case HTTP_VALID: return "HTTP_VALID";
    case HTTP_INVAL_METHOD: return "HTTP_INVAL_METHOD";
    case HTTP_MALFORMED: return "HTTP_MALFORMED";
    case HTTP_VERSION_NOTSUP: return "HTTP_VERSION_NOTSUP";
    case HTTP_NOT_ALLOWED: return "HTTP_NOT_ALLOWED";
    case HTTP_INVAL_STATUS_CODE: return "HTTP_INVAL_STATUS_CODE";
    case HTTP_PATH_TOO_LONG: return "HTTP_PATH_TOO_LONG";
    case HTTP_TOO_MANY_HEADERS: return "HTTP_TOO_MANY_HEADERS";
    case HTTP_HEADER_NAME_TOO_LONG: return "HTTP_HEADER_NAME_TOO_LONG";
    case HTTP_HEADER_VALUE_TOO_LONG: return "HTTP_HEADER_VALUE_TOO_LONG";
    case HTTP_REASON_PHRASE_TOO_LONG: return "HTTP_REASON_PHRASE_TOO_LONG";
    case HTTP_TIMEOUT: return "HTTP_TIMEOUT";
    case HTTP_NOMEM: return "HTTP_NOMEM";
    case HTTP_ILLEGAL_SPACE: return "HTTP_ILLEGAL_SPACE";
    case HTTP_CONTINUE: return "HTTP_CONTINUE";
    case HTTP_SWITCHING_PROTOCOLS: return "HTTP_SWITCHING_PROTOCOLS";
    case HTTP_PROCESSING: return "HTTP_PROCESSING";
    case HTTP_OK: return "HTTP_OK";
    case HTTP_CREATED: return "HTTP_CREATED";
    case HTTP_ACCEPTED: return "HTTP_ACCEPTED";
    case HTTP_NON_AUTHORITATIVE_INFORMATION: return "HTTP_NON_AUTHORITATIVE_INFORMATION";
    case HTTP_NO_CONTENT: return "HTTP_NO_CONTENT";
    case HTTP_RESET_CONTENT: return "HTTP_RESET_CONTENT";
    case HTTP_PARTIAL_CONTENT: return "HTTP_PARTIAL_CONTENT";
    case HTTP_MULTI_STATUS: return "HTTP_MULTI_STATUS";
    case HTTP_ALREADY_REPORTED: return "HTTP_ALREADY_REPORTED";
    case HTTP_IM_USED: return "HTTP_IM_USED";
    case HTTP_MULTIPLE_CHOICES: return "HTTP_MULTIPLE_CHOICES";
    case HTTP_MOVED_PERMANENTLY: return "HTTP_MOVED_PERMANENTLY";
    case HTTP_FOUND: return "HTTP_FOUND";
    case HTTP_SEE_OTHER: return "HTTP_SEE_OTHER";
    case HTTP_NOT_MODIFIED: return "HTTP_NOT_MODIFIED";
    case HTTP_USE_PROXY: return "HTTP_USE_PROXY";
    case HTTP_TEMPORARY_REDIRECT: return "HTTP_TEMPORARY_REDIRECT";
    case HTTP_PERMANENT_REDIRECT: return "HTTP_PERMANENT_REDIRECT";
    case HTTP_BAD_REQUEST: return "HTTP_BAD_REQUEST";
    case HTTP_UNAUTHORIZED: return "HTTP_UNAUTHORIZED";
    case HTTP_PAYMENT_REQUIRED: return "HTTP_PAYMENT_REQUIRED";
    case HTTP_FORBIDDEN: return "HTTP_FORBIDDEN";
    case HTTP_NOT_FOUND: return "HTTP_NOT_FOUND";
    case HTTP_METHOD_NOT_ALLOWED: return "HTTP_METHOD_NOT_ALLOWED";
    case HTTP_NOT_ACCEPTABLE: return "HTTP_NOT_ACCEPTABLE";
    case HTTP_PROXY_AUTHENTICATION_REQUIRED: return "HTTP_PROXY_AUTHENTICATION_REQUIRED";
    case HTTP_REQUEST_TIMEOUT: return "HTTP_REQUEST_TIMEOUT";
    case HTTP_CONFLICT: return "HTTP_CONFLICT";
    case HTTP_GONE: return "HTTP_GONE";
    case HTTP_LENGTH_REQUIRED: return "HTTP_LENGTH_REQUIRED";
    case HTTP_PRECONDITION_FAILED: return "HTTP_PRECONDITION_FAILED";
    case HTTP_PAYLOAD_TOO_LARGE: return "HTTP_PAYLOAD_TOO_LARGE";
    case HTTP_REQUEST_URI_TOO_LONG: return "HTTP_REQUEST_URI_TOO_LONG";
    case HTTP_UNSUPPORTED_MEDIA_TYPE: return "HTTP_UNSUPPORTED_MEDIA_TYPE";
    case HTTP_REQUESTED_RANGE_NOT_SATISFIABLE: return "HTTP_REQUESTED_RANGE_NOT_SATISFIABLE";
    case HTTP_EXPECTATION_FAILED: return "HTTP_EXPECTATION_FAILED";
    case HTTP_IM_A_TEAPOT: return "HTTP_IM_A_TEAPOT";
    case HTTP_MISDIRECTED_REQUEST: return "HTTP_MISDIRECTED_REQUEST";
    case HTTP_UNPROCESSABLE_ENTITY: return "HTTP_UNPROCESSABLE_ENTITY";
    case HTTP_LOCKED: return "HTTP_LOCKED";
    case HTTP_FAILED_DEPENDENCY: return "HTTP_FAILED_DEPENDENCY";
    case HTTP_UPGRADE_REQUIRED: return "HTTP_UPGRADE_REQUIRED";
    case HTTP_PRECONDITION_REQUIRED: return "HTTP_PRECONDITION_REQUIRED";
    case HTTP_TOO_MANY_REQUESTS: return "HTTP_TOO_MANY_REQUESTS";
    case HTTP_REQUEST_HEADER_FIELDS_TOO_LARGE: return "HTTP_REQUEST_HEADER_FIELDS_TOO_LARGE";
    case HTTP_CONNECTION_CLOSED_WITHOUT_RESPONSE: return "HTTP_CONNECTION_CLOSED_WITHOUT_RESPONSE";
    case HTTP_UNAVAILABLE_FOR_LEGAL_REASONS: return "HTTP_UNAVAILABLE_FOR_LEGAL_REASONS";
    case HTTP_CLIENT_CLOSED_REQUEST: return "HTTP_CLIENT_CLOSED_REQUEST";
    case HTTP_INTERNAL_SERVER_ERROR: return "HTTP_INTERNAL_SERVER_ERROR";
    case HTTP_NOT_IMPLEMENTED: return "HTTP_NOT_IMPLEMENTED";
    case HTTP_BAD_GATEWAY: return "HTTP_BAD_GATEWAY";
    case HTTP_SERVICE_UNAVAILABLE: return "HTTP_SERVICE_UNAVAILABLE";
    case HTTP_GATEWAY_TIMEOUT: return "HTTP_GATEWAY_TIMEOUT";
    case HTTP_HTTP_VERSION_NOT_SUPPORTED: return "HTTP_HTTP_VERSION_NOT_SUPPORTED";
    case HTTP_VARIANT_ALSO_NEGOTIATES: return "HTTP_VARIANT_ALSO_NEGOTIATES";
    case HTTP_INSUFFICIENT_STORAGE: return "HTTP_INSUFFICIENT_STORAGE";
    case HTTP_LOOP_DETECTED: return "HTTP_LOOP_DETECTED";
    case HTTP_NOT_EXTENDED: return "HTTP_NOT_EXTENDED";
    case HTTP_NETWORK_AUTHENTICATION_REQUIRED: return "HTTP_NETWORK_AUTHENTICATION_REQUIRED";
    case HTTP_NETWORK_CONNECT_TIMEOUT_ERROR: return "HTTP_NETWORK_CONNECT_TIMEOUT_ERROR";
    default: return "Unknown error";
  }
}