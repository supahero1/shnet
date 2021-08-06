#include "http_p.h"
#include "compress.h"

#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>

char* http1_default_reason_phrase(const int status) {
  switch(status) {
    case http_s_continue: return "Continue";
    case http_s_switching_protocols: return "Switching Protocols";
    case http_s_processing: return "Processing";
    
    case http_s_ok: return "OK";
    case http_s_created: return "Created";
    case http_s_accepted: return "Accepted";
    case http_s_non_authoritative_information: return "Non-authoritative Information";
    case http_s_no_content: return "No Content";
    case http_s_reset_content: return "Reset Content";
    case http_s_partial_content: return "Partial Content";
    case http_s_multi_status: return "Multi-Status";
    case http_s_already_reported: return "Already Reported";
    case http_s_im_used: return "IM Used";
    
    case http_s_multiple_choices: return "Multiple Choices";
    case http_s_moved_permanently: return "Moved Permanently";
    case http_s_found: return "Found";
    case http_s_see_other: return "See Other";
    case http_s_not_modified: return "Not Modified";
    case http_s_use_proxy: return "Use Proxy";
    case http_s_temporary_redirect: return "Temporary Redirect";
    case http_s_permanent_redirect: return "Permanent Redirect";
    
    case http_s_bad_request: return "Bad Request";
    case http_s_unauthorized: return "Unauthorized";
    case http_s_payment_required: return "Payment Required";
    case http_s_forbidden: return "Forbidden";
    case http_s_not_found: return "Not Found";
    case http_s_method_not_allowed: return "Method Not Allowed";
    case http_s_not_acceptable: return "Not Acceptable";
    case http_s_proxy_authentication_required: return "Proxy Authentication Required";
    case http_s_request_timeout: return "Request Timeout";
    case http_s_conflict: return "Conflict";
    case http_s_gone: return "Gone";
    case http_s_length_required: return "Lengh Required";
    case http_s_precondition_failed: return "Precondition Failed";
    case http_s_payload_too_large: return "Payload Too Large";
    case http_s_request_uri_too_long: return "Request-URI Too Long";
    case http_s_unsupported_media_type: return "Unsupported Media Type";
    case http_s_requested_range_not_satisfiable: return "Requested Range Not Satisfiable";
    case http_s_expectation_failed: return "Expectation Failed";
    case http_s_i_am_a_teapot: return "I'm a teapot";
    case http_s_misdirected_request: return "Misdirected Request";
    case http_s_unprocessable_entity: return "Unprocessable Entity";
    case http_s_locked: return "Locked";
    case http_s_failed_dependency: return "Failed Dependency";
    case http_s_upgrade_required: return "Upgrade Required";
    case http_s_precondition_required: return "Precondition Required";
    case http_s_too_many_requests: return "Too Many Requests";
    case http_s_request_header_fields_too_large: return "Request Header Fields Too Large";
    case http_s_connection_closed_without_response: return "Connection Closed Without Response";
    case http_s_unavailable_for_legal_reasons: return "Unavailable For Legal Reasons";
    case http_s_client_closed_request: return "Client Closed Request";
    
    case http_s_internal_server_error: return "Internal Server Error";
    case http_s_not_implemented: return "Not Implemented";
    case http_s_bad_gateway: return "Bad Gateway";
    case http_s_service_unavailable: return "Service Unavailable";
    case http_s_gateway_timeout: return "Gateway Timeout";
    case http_s_http_version_not_supported: return "HTTP Version Not Supported";
    case http_s_variant_also_negotiates: return "Variant Also Negotiates";
    case http_s_insufficient_storage: return "Insufficient Storage";
    case http_s_loop_detected: return "Loop Detected";
    case http_s_not_extended: return "Not Extended";
    case http_s_network_authentication_required: return "Network Authentication Required";
    case http_s_network_connect_timeout_error: return "Network Connect Timeout Error";
    
    default: return "";
  }
}

uint32_t http1_default_reason_phrase_len(const int status) {
  switch(status) {
    case http_s_continue: return 8;
    case http_s_switching_protocols: return 19;
    case http_s_processing: return 10;
    
    case http_s_ok: return 2;
    case http_s_created: return 7;
    case http_s_accepted: return 8;
    case http_s_non_authoritative_information: return 29;
    case http_s_no_content: return 10;
    case http_s_reset_content: return 13;
    case http_s_partial_content: return 15;
    case http_s_multi_status: return 12;
    case http_s_already_reported: return 16;
    case http_s_im_used: return 7;
    
    case http_s_multiple_choices: return 16;
    case http_s_moved_permanently: return 17;
    case http_s_found: return 5;
    case http_s_see_other: return 9;
    case http_s_not_modified: return 12;
    case http_s_use_proxy: return 9;
    case http_s_temporary_redirect: return 18;
    case http_s_permanent_redirect: return 18;
    
    case http_s_bad_request: return 11;
    case http_s_unauthorized: return 12;
    case http_s_payment_required: return 16;
    case http_s_forbidden: return 9;
    case http_s_not_found: return 9;
    case http_s_method_not_allowed: return 18;
    case http_s_not_acceptable: return 14;
    case http_s_proxy_authentication_required: return 29;
    case http_s_request_timeout: return 15;
    case http_s_conflict: return 8;
    case http_s_gone: return 4;
    case http_s_length_required: return 14;
    case http_s_precondition_failed: return 19;
    case http_s_payload_too_large: return 17;
    case http_s_request_uri_too_long: return 20;
    case http_s_unsupported_media_type: return 22;
    case http_s_requested_range_not_satisfiable: return 31;
    case http_s_expectation_failed: return 18;
    case http_s_i_am_a_teapot: return 12;
    case http_s_misdirected_request: return 19;
    case http_s_unprocessable_entity: return 20;
    case http_s_locked: return 6;
    case http_s_failed_dependency: return 17;
    case http_s_upgrade_required: return 16;
    case http_s_precondition_required: return 21;
    case http_s_too_many_requests: return 17;
    case http_s_request_header_fields_too_large: return 31;
    case http_s_connection_closed_without_response: return 34;
    case http_s_unavailable_for_legal_reasons: return 29;
    case http_s_client_closed_request: return 21;
    
    case http_s_internal_server_error: return 21;
    case http_s_not_implemented: return 15;
    case http_s_bad_gateway: return 11;
    case http_s_service_unavailable: return 19;
    case http_s_gateway_timeout: return 15;
    case http_s_http_version_not_supported: return 26;
    case http_s_variant_also_negotiates: return 23;
    case http_s_insufficient_storage: return 20;
    case http_s_loop_detected: return 13;
    case http_s_not_extended: return 12;
    case http_s_network_authentication_required: return 31;
    case http_s_network_connect_timeout_error: return 29;
    
    default: return 0;
  }
}

static const char http_tokens[] = {
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

static const char http_hex[] = {
  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
  0,  1,  2,  3,  4,  5,  6,  7,  8,  9,  0,  0,  0,  0,  0,  0,
  0, 10, 11, 12, 13, 14, 15,  0,  0,  0,  0,  0,  0,  0,  0,  0,
  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
  0, 10, 11, 12, 13, 14, 15
};

#define IS_TOKEN(a) (http_tokens[(int)(a)])
#define HEX_TO_NUM(a) (http_hex[(int)(a)])
#define IS_DIGIT(a) ((a) >= '0' && (a) <= '9')
#define IS_HEX(a) (IS_DIGIT(a) || ((a) >= 'A' && (a) <= 'F') || ((a) >= 'a' && (a) <= 'f'))

const char* http1_parser_strerror(const int err) {
  switch(err) {
    case http_valid: return "http_valid";
    case http_incomplete: return "http_incomplete";
    case http_no_method: return "http_no_method";
    case http_method_too_long: return "http_method_too_long";
    case http_invalid_character: return "http_invalid_character";
    case http_no_path: return "http_no_path";
    case http_path_too_long: return "http_path_too_long";
    case http_invalid_version: return "http_invalid_version";
    case http_too_many_headers: return "http_too_many_headers";
    case http_header_name_too_long: return "http_header_name_too_long";
    case http_no_header_name: return "http_no_header_name";
    case http_header_value_too_long: return "http_header_value_too_long";
    case http_body_too_long: return "http_body_too_long";
    case http_transfer_not_supported: return "http_transfer_not_supported";
    case http_encoding_not_supported: return "http_encoding_not_supported";
    case http_out_of_memory: return "http_out_of_memory";
    case http_corrupted_body_compression: return "http_corrupted_body_compression";
    case http_no_chunk_size: return "http_no_chunk_size";
    case http_invalid_status_code: return "http_invalid_status_code";
    case http_reason_phrase_too_long: return "http_reason_phrase_too_long";
    case http_invalid: return "http_invalid";
    case http_closed: return "http_closed";
    default: return "http_unknown_error";
  }
}

uint64_t http1_message_length(const struct http_message* const message) {
  if(message->method != NULL) { /* Request */
    /* 2 spaces, 2 CRLF, sizeof HTTP/1.1  = 14 */
    uint64_t length = message->method_len + 14 + message->path_len + message->body_len + message->headers_len * 4;
    for(uint32_t i = 0; i < message->headers_len; ++i) {
      length += message->headers[i].name_len + message->headers[i].value_len;
    }
    return length;
  } else { /* Response */
    /* sizeof HTTP/1.1, 2 SP, 3 numbers, 2 CLRF  = 17 */
    uint64_t length = message->reason_phrase_len + 17 + message->body_len + message->headers_len * 4;
    for(uint32_t i = 0; i < message->headers_len; ++i) {
      length += message->headers[i].name_len + message->headers[i].value_len;
    }
    return length;
  }
}

/* Buffer must be at least http1_message_length(message) bytes long */

void http1_create_message(char* buffer, const struct http_message* const message) {
  if(message->method != NULL) {
    (void) memcpy(buffer, message->method, message->method_len);
    buffer += message->method_len;
    *buffer = ' ';
    ++buffer;
    (void) memcpy(buffer, message->path, message->path_len);
    buffer += message->path_len;
    (void) memcpy(buffer, " HTTP/1.1\r\n", 11);
    buffer += 11;
  } else {
    (void) memcpy(buffer, "HTTP/1.1 ", 9);
    buffer += 9;
    *buffer = 48 + message->status_code / 100;
    ++buffer;
    *buffer = 48 + (message->status_code % 100) / 10;
    ++buffer;
    *buffer = 48 + message->status_code % 10;
    ++buffer;
    *buffer = ' ';
    ++buffer;
    if(message->reason_phrase != NULL) {
      (void) memcpy(buffer, message->reason_phrase, message->reason_phrase_len);
      buffer += message->reason_phrase_len;
    }
    *buffer = '\r';
    ++buffer;
    *buffer = '\n';
    ++buffer;
  }
  for(uint32_t i = 0; i < message->headers_len; ++i) {
    (void) memcpy(buffer, message->headers[i].name, message->headers[i].name_len);
    buffer += message->headers[i].name_len;
    *buffer = ':';
    ++buffer;
    *buffer = ' ';
    ++buffer;
    (void) memcpy(buffer, message->headers[i].value, message->headers[i].value_len);
    buffer += message->headers[i].value_len;
    *buffer = '\r';
    ++buffer;
    *buffer = '\n';
    ++buffer;
  }
  *buffer = '\r';
  ++buffer;
  *buffer = '\n';
  ++buffer;
  if(message->body != NULL) {
    (void) memcpy(buffer, message->body, message->body_len);
  }
}


#define ATLEAST(a) \
do { \
  if((a) > size) return http_incomplete; \
} while(0)

static int http1_parse_headers_and_body(char* buffer, uint64_t size, struct http_parser_session* const session,
  const struct http_parser_settings* const settings, struct http_message* const message) {
  uint64_t idx = 0;
  if(session->last_at == http_pla_body) {
    goto pla_body;
  }
  if(settings->max_headers == 0 || message->headers_len == 0) {
    ATLEAST(2);
    if(buffer[0] == '\r') {
      session->last_idx += 2;
      buffer += 2;
      size -= 2;
      message->headers_len = 0;
    } else {
      return http_too_many_headers;
    }
  } else {
    uint32_t i = session->last_header_idx;
    const uint32_t s = message->headers_len < settings->max_headers ? message->headers_len : settings->max_headers;
    for(; i < s; ++i) {
      session->last_header_idx = i;
      ATLEAST(2);
      if(buffer[0] == '\r') {
        session->last_idx += 2;
        buffer += 2;
        size -= 2;
        break;
      }
      /* Header name */
      ATLEAST(5);
      if(!settings->no_character_validation) {
        uint64_t j = 0;
        const uint64_t smaller = size < settings->max_header_name_len + 1 ? size : settings->max_header_name_len + 2;
        for(; j < smaller; ++j) {
          if(!IS_TOKEN(buffer[j])) {
            if(buffer[j] == ':') {
              break;
            } else {
              return http_invalid_character;
            }
          }
        }
        if(j > settings->max_header_name_len) {
          return http_header_name_too_long;
        }
        if(j == size) {
          return http_incomplete;
        }
        if(j == 0) {
          return http_no_header_name;
        }
        message->headers[i].name = buffer;
        message->headers[i].name_len = j;
        idx = j + 1;
      } else {
        const uint64_t smaller = size < settings->max_header_name_len + 1 ? size : settings->max_header_name_len + 1;
        const char* const colon = memchr(buffer, ':', smaller);
        if(colon == NULL) {
          if(size > settings->max_header_name_len) {
            return http_header_name_too_long;
          }
          return http_incomplete;
        }
        message->headers[i].name_len = (uintptr_t) colon - (uintptr_t) buffer;
        if(message->headers[i].name_len > settings->max_header_name_len) {
          return http_header_name_too_long;
        }
        if(message->headers[i].name_len == 0) {
          return http_no_header_name;
        }
        message->headers[i].name = buffer;
        idx = message->headers[i].name_len + 1;
      }
      size -= idx;
      ATLEAST(3);
      buffer += idx + 1;
      --size;
      uint64_t whole_idx = idx + 1;
      /* Header value */
      if(!settings->no_character_validation) {
        uint64_t j = 0;
        const uint64_t smaller = size < settings->max_header_value_len + 1 ? size : settings->max_header_value_len + 1;
        for(; j < smaller; ++j) {
          if(buffer[j] < 32 || buffer[j] > 126) {
            if(buffer[j] == '\r') {
              break;
            } else {
              return http_invalid_character;
            }
          }
        }
        if(j > settings->max_header_value_len) {
          return http_header_value_too_long;
        }
        if(j == size) {
          return http_incomplete;
        }
        message->headers[i].value = buffer;
        message->headers[i].value_len = j;
        idx = j + 1;
      } else {
        const uint64_t smaller = size < settings->max_header_value_len + 1 ? size : settings->max_header_value_len + 1;
        const char* const cr = memchr(buffer, '\r', smaller);
        if(cr == NULL) {
          if(size > settings->max_header_value_len) {
            return http_header_value_too_long;
          }
          return http_incomplete;
        }
        message->headers[i].value_len = (uintptr_t) cr - (uintptr_t) buffer;
        if(message->headers[i].value_len > settings->max_header_value_len) {
          return http_header_value_too_long;
        }
        /* Header value length can be 0, so no check for that */
        message->headers[i].value = buffer;
        idx = message->headers[i].value_len + 1;
      }
      if(!settings->no_body_parsing) {
        if(message->headers[i].name_len == 14 && strncasecmp(message->headers[i].name, "content-length", 14) == 0) {
          if(message->headers[i].value_len > 20) {
            /* Certainly bigger than maximum 64bit value */
            return http_body_too_long;
          }
          const char save = message->headers[i].value[message->headers[i].value_len];
          message->headers[i].value[message->headers[i].value_len] = 0;
          const uint64_t body_len = atoll(message->headers[i].value);
          message->headers[i].value[message->headers[i].value_len] = save;
          if(body_len > settings->max_body_len) {
            return http_body_too_long;
          }
          message->body_len = body_len;
        } else if(message->headers[i].name_len == 17 && strncasecmp(message->headers[i].name, "transfer-encoding", 17) == 0) {
          if(message->headers[i].value_len == 7 && strncasecmp(message->headers[i].value, "chunked", 7) == 0) {
            message->transfer = http_t_chunked;
          } else if(message->headers[i].value_len != 4 || strncasecmp(message->headers[i].value, "none", 4) != 0) {
            return http_transfer_not_supported;
          }
        } else if(message->headers[i].name_len == 16 && strncasecmp(message->headers[i].name, "content-encoding", 16) == 0) {
          if(message->headers[i].value_len == 4 && strncasecmp(message->headers[i].value, "gzip", 4) == 0) {
            if((settings->dont_accept_encoding & http_ae_gzip) == 0) {
              message->encoding = http_e_gzip;
            } else {
              return http_encoding_not_supported;
            }
          } else if(message->headers[i].value_len == 7 && strncasecmp(message->headers[i].value, "deflate", 7) == 0) {
            if((settings->dont_accept_encoding & http_ae_deflate) == 0) {
              message->encoding = http_e_deflate;
            } else {
              return http_encoding_not_supported;
            }
          } else if(message->headers[i].value_len == 2 && strncasecmp(message->headers[i].value, "br", 2) == 0) {
            if((settings->dont_accept_encoding & http_ae_brotli) == 0) {
              message->encoding = http_e_brotli;
            } else {
              return http_encoding_not_supported;
            }
          } else if(message->headers[i].value_len != 0 && (message->headers[i].value_len != 4 || strncasecmp(message->headers[i].value, "none", 4) != 0)) {
            return http_encoding_not_supported;
          } /* else it's empty and let's just ignore it */
        }
      }
      size -= idx;
      ATLEAST(1);
      buffer += idx + 1;
      --size;
      whole_idx += idx + 1;
      session->last_idx += whole_idx;
      session->last_header_idx = i + 1;
      if(settings->stop_at_every_header) {
        return http_valid;
      }
    }
    if(i > settings->max_headers) {
      return http_too_many_headers;
    }
    message->headers_len = i;
  }
  if(message->transfer == http_t_chunked) {
    message->body_len = 0;
  }
  session->last_at = http_pla_body;
  session->parsed_headers = 1;
  if(settings->stop_at_headers) {
    return http_valid;
  }
  pla_body:
  if(message->transfer == http_t_none) {
    if(message->body_len == 0) {
      /* No body */
      session->parsed_body = 1;
      return http_valid;
    }
    ATLEAST(message->body_len);
    if(settings->no_processing) {
      /* Now that we have the entire body, we can leave */
      session->parsed_body = 1;
      return http_valid;
    }
    if(message->encoding == http_e_none) {
      message->body = buffer;
      session->parsed_body = 1;
      return http_valid;
    }
    decode:;
    size_t len;
    switch(message->encoding) {
      case http_e_gzip: {
        message->body = gunzip(NULL, buffer, message->body_len, NULL, &len, settings->max_body_len, 0);
        break;
      }
      case http_e_deflate: {
        message->body = inflate_(NULL, buffer, message->body_len, NULL, &len, settings->max_body_len, 0);
        break;
      }
      case http_e_brotli: {
        message->body = brotli_decompress(buffer, message->body_len, &len, settings->max_body_len);
        break;
      }
      default: __builtin_unreachable();
    }
    if(message->body == NULL) {
      if(errno == ENOMEM) {
        return http_out_of_memory;
      } else if(errno == EOVERFLOW) {
        return http_body_too_long;
      } else {
        return http_corrupted_body_compression;
      }
    }
    void* const ptr = realloc(message->body, len);
    if(ptr != NULL) {
      message->body = ptr;
    }
    message->body_len = len;
    message->alloc_body = 1;
    /* No encoding now */
    message->encoding = http_e_none;
  } else {
    while(1) {
      uint64_t chunk_len = 0;
      uint64_t i = 0;
      ATLEAST(3);
      for(; i < size; ++i) {
        if(!IS_HEX(buffer[i])) {
          if(buffer[i] == '\r') {
            break;
          } else {
            return http_invalid_character;
          }
        }
        chunk_len <<= 4;
        chunk_len |= HEX_TO_NUM(buffer[i]);
        if(chunk_len > settings->max_body_len || message->body_len + chunk_len > settings->max_body_len) {
          return http_body_too_long;
        }
      }
      if(i == size) {
        return http_incomplete;
      }
      if(i == 0) {
        return http_no_chunk_size;
      }
      size -= i + 1;
      ATLEAST(1);
      buffer += i + 2;
      --size;
      ATLEAST(chunk_len + 2);
      session->last_idx += i + 2;
      if(chunk_len == 0) {
        if(message->body_len > 0 && !settings->no_processing) {
          /* So that if we resume the session, we won't end up here */
          message->transfer = http_t_none;
          if(message->encoding != http_e_none) {
            size = message->body_len;
            buffer = message->body;
            goto decode;
          } else {
            session->parsed_body = 1;
            return http_valid;
          }
        } else {
          session->parsed_body = 1;
          return http_valid;
        }
      }
      if(!settings->no_processing) {
        session->chunk_idx += i + 4;
        (void) memmove(buffer - session->chunk_idx, buffer, chunk_len);
        if(message->body == NULL) {
          message->body = buffer - session->chunk_idx;
        }
      } else if(message->body == NULL) {
        message->body = buffer;
      }
      buffer += chunk_len + 2;
      size -= chunk_len + 2;
      session->last_idx += chunk_len + 2;
      message->body_len += chunk_len;
    }
  }
  session->parsed_body = 1;
  return http_valid;
}

/* message->headers and message->headers_len must be initialised */

int http1_parse_request(char* buffer, uint64_t size, struct http_parser_session* const session,
  const struct http_parser_settings* const settings, struct http_message* const message) {
  buffer += session->last_idx;
  size -= session->last_idx;
  switch(session->last_at) {
    case http_pla_method: break;
    case http_pla_path: goto pla_path;
    case http_pla_version: goto pla_version;
    case http_pla_body:
    case http_pla_headers: goto pla_headers;
    default: __builtin_unreachable();
  }
  ATLEAST(2);
  uint64_t idx;
  message->method = buffer;
  if(!settings->no_character_validation) {
    uint64_t i = 0;
    const uint64_t smaller = size < settings->max_method_len + 1 ? size : settings->max_method_len + 1;
    for(; i < smaller; ++i) {
      if(!IS_TOKEN(buffer[i])) {
        if(buffer[i] == ' ') {
          break;
        } else {
          return http_invalid_character;
        }
      }
    }
    if(i > settings->max_method_len) {
      return http_method_too_long;
    }
    if(i == size) {
      return http_incomplete;
    }
    if(i == 0) {
      return http_no_method;
    }
    message->method_len = i;
    idx = i + 1;
  } else {
    const uint64_t smaller = size < settings->max_method_len + 1 ? size : settings->max_method_len + 1;
    const char* const space = memchr(buffer, ' ', smaller);
    if(space == NULL) {
      if(size > settings->max_method_len) {
        return http_method_too_long;
      }
      return http_incomplete;
    }
    const uint32_t method_len = (uintptr_t) space - (uintptr_t) buffer;
    if(method_len > settings->max_method_len) {
      return http_method_too_long;
    }
    if(method_len == 0) {
      return http_no_method;
    }
    message->method_len = method_len;
    idx = method_len + 1;
  }
  session->last_at = http_pla_path;
  session->parsed_method = 1;
  session->last_idx = idx;
  if(settings->stop_at_method) {
    return http_valid;
  }
  buffer += idx;
  size -= idx;
  pla_path:
  /* Don't validate the path here, let the user choose what to do with it.
  That's because some filesystems have reserved characters for filenames, others
  don't. */
  {
    const uint64_t smaller = size < settings->max_path_len + 1 ? size : settings->max_path_len + 1;
    const char* const space = memchr(buffer, ' ', smaller);
    if(space == NULL) {
      if(size > settings->max_path_len) {
        return http_path_too_long;
      }
      return http_incomplete;
    }
    const uint32_t path_len = (uintptr_t) space - (uintptr_t) buffer;
    if(path_len > settings->max_path_len) {
      return http_path_too_long;
    }
    if(path_len == 0) {
      return http_no_path;
    }
    message->path = buffer;
    message->path_len = path_len;
    idx = path_len + 1;
  }
  session->last_at = http_pla_version;
  session->parsed_path = 1;
  session->last_idx += idx;
  if(settings->stop_at_path) {
    return http_valid;
  }
  buffer += idx;
  size -= idx;
  pla_version:
  ATLEAST(10);
  /* Don't make a new "http_malformed_version" error if "HTTP/" isn't there */
  if(memcmp(buffer, "HTTP/1.1", 8) != 0) {
    return http_invalid_version;
  }
  /* We don't care if \r\n is there. It is only important for readability. */
  session->last_at = http_pla_headers;
  session->parsed_version = 1;
  session->last_idx += 10;
  if(settings->stop_at_version) {
    return http_valid;
  }
  buffer += 10;
  size -= 10;
  pla_headers:
  return http1_parse_headers_and_body(buffer, size, session, settings, message);
}

int http1_parse_response(char* buffer, uint64_t size, struct http_parser_session* const session,
  const struct http_parser_settings* const settings, struct http_message* const message) {
  buffer += session->last_idx;
  size -= session->last_idx;
  switch(session->last_at) {
    case http_pla_res_version: break;
    case http_pla_status_code: goto pla_status_code;
    case http_pla_reason_phrase: goto pla_reason_phrase;
    case http_pla_body:
    case http_pla_headers: goto pla_headers;
    default: __builtin_unreachable();
  }
  ATLEAST(9);
  if(memcmp(buffer, "HTTP/1.1", 8) != 0) {
    return http_invalid_version;
  }
  session->last_at = http_pla_status_code;
  session->parsed_version = 1;
  session->last_idx += 9;
  if(settings->stop_at_version) {
    return http_valid;
  }
  buffer += 9;
  size -= 9;
  pla_status_code:
  ATLEAST(3);
  if(!IS_DIGIT(buffer[0]) || !IS_DIGIT(buffer[1]) || !IS_DIGIT(buffer[2])) {
    return http_invalid_character;
  }
  message->status_code = (buffer[0] - 48) * 100 + (buffer[1] - 48) * 10 + buffer[2] - 48;
  if(message->status_code < 100) {
    return http_invalid_status_code;
  }
  session->last_at = http_pla_reason_phrase;
  session->parsed_status_code = 1;
  session->last_idx += 3;
  if(settings->stop_at_status_code) {
    return http_valid;
  }
  buffer += 3;
  size -= 3;
  pla_reason_phrase:
  ATLEAST(2);
  uint8_t add = 0;
  /* Skip the ambiguous space, should it be here or not? */
  if(buffer[0] == ' ') {
    ++buffer;
    --size;
    add = 1;
  }
  ATLEAST(2);
  uint64_t idx;
  if(!settings->no_character_validation) {
    uint64_t i = 0;
    const uint64_t smaller = size < settings->max_reason_phrase_len + 1 ? size : settings->max_reason_phrase_len + 1;
    for(; i < smaller; ++i) {
      if(buffer[i] < 32 || buffer[i] > 126) {
        if(buffer[i] == '\r') {
          break;
        } else {
          return http_invalid_character;
        }
      }
    }
    if(i > settings->max_reason_phrase_len) {
      return http_reason_phrase_too_long;
    }
    if(i == size) {
      return http_incomplete;
    }
    message->reason_phrase = buffer;
    message->reason_phrase_len = i;
    idx = i + 1;
  } else {
    const uint64_t smaller = size < settings->max_reason_phrase_len + 1 ? size : settings->max_reason_phrase_len + 1;
    const char* const cr = memchr(buffer, '\r', smaller);
    if(cr == NULL) {
      if(size > settings->max_reason_phrase_len) {
        return http_reason_phrase_too_long;
      }
      return http_incomplete;
    }
    message->reason_phrase_len = (uintptr_t) cr - (uintptr_t) buffer;
    if(message->reason_phrase_len > settings->max_reason_phrase_len) {
      return http_reason_phrase_too_long;
    }
    message->reason_phrase = buffer;
    idx = message->reason_phrase_len + 1;
  }
  size -= idx;
  ATLEAST(1);
  buffer += idx + 1;
  --size;
  session->last_at = http_pla_headers;
  session->parsed_reason_phrase = 1;
  session->last_idx += idx + 1 + add;
  if(settings->stop_at_reason_phrase) {
    return http_valid;
  }
  pla_headers:
  return http1_parse_headers_and_body(buffer, size, session, settings, message);
}

struct http_header* http1_seek_header(const struct http_message* const message, const char* const name, const uint32_t len) {
  for(uint32_t i = 0; i < message->headers_len; ++i) {
    if(message->headers[i].name_len == len && strncasecmp(message->headers[i].name, name, len) == 0) {
      return message->headers + i;
    }
  }
  return NULL;
}

#undef ATLEAST

#define old (uintptr_t) old_ptr
#define new (uintptr_t) new_ptr

void http1_convert_message(const void* const old_ptr, const void* const new_ptr, struct http_message* const message) {
  if(message->method != NULL) {
    message->method = (char*)(new + ((uintptr_t) message->method - old));
  }
  if(message->path != NULL) {
    message->path = (char*)(new + ((uintptr_t) message->path - old));
  }
  if(message->body != NULL) {
    message->body = (char*)(new + ((uintptr_t) message->body - old));
  }
  if(message->headers == NULL) {
    return;
  }
  for(uint8_t i = 0; i < message->headers_len; ++i) {
    if(message->headers[i].name != NULL) {
      message->headers[i].name = (char*)(new + ((uintptr_t) message->headers[i].name - old));
    }
    if(message->headers[i].value != NULL) {
      message->headers[i].value = (char*)(new + ((uintptr_t) message->headers[i].value - old));
    }
  }
}

#undef new
#undef old