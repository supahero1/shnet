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

#ifndef MyxpZpo4_G3__8_gF__x___zpu_W5ukt
#define MyxpZpo4_G3__8_gF__x___zpu_W5ukt 1

#ifdef __cplusplus
extern "C" {
#endif

#include "net.h"

enum HTTP_codes {
  HTTP_GET,
  HTTP_HEAD,
  HTTP_POST,
  HTTP_PUT,
  HTTP_DELETE,
  HTTP_TRACE,
  HTTP_OPTIONS,
  HTTP_CONNECT,
  HTTP_PATCH,
  
  // parsing options
  
  HTTP_PARSE_METHOD = 1,
  HTTP_PARSE_STATUS = 1,
  HTTP_PARSE_REASON_PHRASE,
  HTTP_IGNORE_REASON_PHRASE = 4,
  HTTP_PARSE_PATH = 2,
  HTTP_PARSE_VERSION = 4,
  HTTP_PARSE_HEADERS = 8,
  HTTP_PARSE_BODY = 16,
  HTTP_ALLOW_REPETITIVE_HEADER_WHITESPACE = 32,
  
  // creating options
  
  HTTP_DATA_MIGHT_OVERLAP = 1,
  // HTTP_CHUNK,
  
  // parsing return values
  
  HTTP_VALID = 0,
  HTTP_INVAL_METHOD,
  HTTP_MALFORMED,
  HTTP_VERSION_NOTSUP,
  HTTP_NOT_ALLOWED,
  HTTP_INVAL_STATUS_CODE,
  HTTP_PATH_TOO_LONG,
  HTTP_TOO_MANY_HEADERS,
  HTTP_HEADER_NAME_TOO_LONG,
  HTTP_HEADER_VALUE_TOO_LONG,
  HTTP_REASON_PHRASE_TOO_LONG,
  HTTP_TIMEOUT,
  HTTP_NOMEM,
  HTTP_ILLEGAL_SPACE,
  
  // status codes (extended)
  
  HTTP_CONTINUE = 100,
  HTTP_SWITCHING_PROTOCOLS,
  HTTP_PROCESSING,
  
  HTTP_OK = 200,
  HTTP_CREATED,
  HTTP_ACCEPTED,
  HTTP_NON_AUTHORITATIVE_INFORMATION,
  HTTP_NO_CONTENT,
  HTTP_RESET_CONTENT,
  HTTP_PARTIAL_CONTENT,
  HTTP_MULTI_STATUS,
  HTTP_ALREADY_REPORTED,
  HTTP_IM_USED = 226,
  
  HTTP_MULTIPLE_CHOICES = 300,
  HTTP_MOVED_PERMANENTLY,
  HTTP_FOUND,
  HTTP_SEE_OTHER,
  HTTP_NOT_MODIFIED,
  HTTP_USE_PROXY,
  HTTP_TEMPORARY_REDIRECT = 307,
  HTTP_PERMANENT_REDIRECT,
  
  HTTP_BAD_REQUEST = 400,
  HTTP_UNAUTHORIZED,
  HTTP_PAYMENT_REQUIRED,
  HTTP_FORBIDDEN,
  HTTP_NOT_FOUND,
  HTTP_METHOD_NOT_ALLOWED,
  HTTP_NOT_ACCEPTABLE,
  HTTP_PROXY_AUTHENTICATION_REQUIRED,
  HTTP_REQUEST_TIMEOUT,
  HTTP_CONFLICT,
  HTTP_GONE,
  HTTP_LENGTH_REQUIRED,
  HTTP_PRECONDITION_FAILED,
  HTTP_PAYLOAD_TOO_LARGE,
  HTTP_REQUEST_URI_TOO_LONG,
  HTTP_UNSUPPORTED_MEDIA_TYPE,
  HTTP_REQUESTED_RANGE_NOT_SATISFIABLE,
  HTTP_EXPECTATION_FAILED,
  HTTP_IM_A_TEAPOT,
  HTTP_MISDIRECTED_REQUEST = 421,
  HTTP_UNPROCESSABLE_ENTITY,
  HTTP_LOCKED,
  HTTP_FAILED_DEPENDENCY,
  HTTP_UPGRADE_REQUIRED = 426,
  HTTP_PRECONDITION_REQUIRED = 428,
  HTTP_TOO_MANY_REQUESTS,
  HTTP_REQUEST_HEADER_FIELDS_TOO_LARGE = 431,
  HTTP_CONNECTION_CLOSED_WITHOUT_RESPONSE = 444,
  HTTP_UNAVAILABLE_FOR_LEGAL_REASONS = 451,
  HTTP_CLIENT_CLOSED_REQUEST = 499,
  
  HTTP_INTERNAL_SERVER_ERROR = 500,
  HTTP_NOT_IMPLEMENTED,
  HTTP_BAD_GATEWAY,
  HTTP_SERVICE_UNAVAILABLE,
  HTTP_GATEWAY_TIMEOUT,
  HTTP_HTTP_VERSION_NOT_SUPPORTED,
  HTTP_VARIANT_ALSO_NEGOTIATES,
  HTTP_INSUFFICIENT_STORAGE,
  HTTP_LOOP_DETECTED,
  HTTP_NOT_EXTENDED = 510,
  HTTP_NETWORK_AUTHENTICATION_REQUIRED,
  HTTP_NETWORK_CONNECT_TIMEOUT_ERROR = 599
};

struct HTTP_header {
  char* name;
  char* value;
  uint32_t name_length;
  uint32_t value_length;
};

struct HTTP_request {
  struct HTTP_header* headers;
  uint8_t* body;
  char* path;
  uint32_t header_amount;
  uint32_t body_length;
  uint32_t path_length;
  int method;
};

struct HTTP_settings {
  uint32_t max_path_length;          // 4096
  uint32_t max_header_amount;        // 64
  uint32_t max_header_name_length;   // 64
  uint32_t max_header_value_length;  // 4096
  uint32_t max_reason_phrase_length; // 64
  uint32_t max_body_length;          // 256 * 4096 (1.05MB)
};

extern struct HTTP_settings HTTP_default_settings(void);

struct HTTP_parser_session {
  uint32_t idx;
  int last_at;
};

extern int HTTPv1_1_request_parser(uint8_t* const, const uint32_t, const int, struct HTTP_request* const, const struct HTTP_settings* const, struct HTTP_parser_session* const);

struct HTTP_response {
  struct HTTP_header* headers;
  uint8_t* body;
  char* reason_phrase;
  uint32_t header_amount;
  uint32_t body_length;
  uint32_t status_code;
  uint32_t reason_phrase_length;
};

extern int HTTPv1_1_response_parser(uint8_t* const, const uint32_t, const int, struct HTTP_response* const, const struct HTTP_settings* const, struct HTTP_parser_session* const);

extern char* HTTP_strerror(const int);

extern struct HTTP_header* HTTP_get_header(struct HTTP_header* const, const uint32_t, const char* const, const uint32_t);

extern uint32_t HTTP_get_method_length(const int);

extern char* HTTP_get_method_name(const int);

extern uint32_t HTTP_request_size(const struct HTTP_request* const);

extern uint32_t HTTP_create_request(char** const, const uint32_t, const int, const struct HTTP_request* const);

extern uint32_t HTTP_response_size(const struct HTTP_response* const);

extern uint32_t HTTP_create_response(char** const, const uint32_t, const int, const struct HTTP_response* const);

#ifdef __cplusplus
}
#endif

#endif // MyxpZpo4_G3__8_gF__x___zpu_W5ukt