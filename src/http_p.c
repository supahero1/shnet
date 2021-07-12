#include "http_p.h"
#include "compress.h"

#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>

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
  0,  10, 11, 12, 13, 14, 15, 0,  0,  0,  0,  0,  0,  0,  0,  0,
  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
  0,  10, 11, 12, 13, 14, 15
};

#define IS_TOKEN(a) (http_tokens[(int)(a)])
#define HEX_TO_NUM(a) (http_hex[(int)(a)])

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
    case http_has_headers_but_max_is_0: return "http_has_headers_but_max_is_0";
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
    default: return "http_unknown_error";
  }
}

uint32_t http1_message_length(const struct http_message* const message) {
  if(message->method != NULL) { /* Request */
    /* 2 spaces, 2 CRLF, sizeof HTTP/1.1  = 14 */
    uint32_t length = message->method_len + 14 + message->path_len + message->body_len + message->headers_len * 4;
    for(uint32_t i = 0; i < message->headers_len; ++i) {
      length += message->headers[i].name_len + message->headers[i].value_len;
    }
    return length;
  } else { /* Response */
    /* sizeof HTTP/1.1, 2 SP, 3 numbers, 2 CLRF  = 17 */
    uint32_t length = message->reason_phrase_len + 17 + message->body_len + message->headers_len * 4;
    for(uint32_t i = 0; i < message->headers_len; ++i) {
      length += message->headers[i].name_len + message->headers[i].value_len;
    }
    return length;
  }
}

/* Buffer must be at least http1_message_length(message) bytes long */

void http1_create_message(char* buffer, const struct http_message* const message) {
  if(message->method == NULL) {
    (void) memcpy(buffer, message->method, message->method_len);
    buffer += message->method_len;
    buffer[0] = ' ';
    ++buffer;
    buffer[0] = '/';
    ++buffer;
    (void) memcpy(buffer, message->path, message->path_len);
    buffer += message->path_len;
    (void) memcpy(buffer, " HTTP/1.1\r\n", 11);
    buffer += 11;
  } else {
    (void) memcpy(buffer, "HTTP/1.1 ", 9);
    buffer += 9;
    buffer[0] = 48 + message->status_code / 100;
    ++buffer;
    buffer[0] = 48 + (message->status_code % 100) / 10;
    ++buffer;
    buffer[0] = 48 + message->status_code % 10;
    ++buffer;
    buffer[0] = ' ';
    ++buffer;
    if(message->reason_phrase != NULL) {
      (void) memcpy(buffer, message->reason_phrase, message->reason_phrase_len);
      buffer += message->reason_phrase_len;
    }
    buffer[0] = '\r';
    ++buffer;
    buffer[0] = '\n';
    ++buffer;
  }
  for(uint32_t i = 0; i < message->headers_len; ++i) {
    (void) memcpy(buffer, message->headers[i].name, message->headers[i].name_len);
    buffer += message->headers[i].name_len;
    buffer[0] = ':';
    ++buffer;
    buffer[0] = ' ';
    ++buffer;
    (void) memcpy(buffer, message->headers[i].value, message->headers[i].value_len);
    buffer += message->headers[i].value_len;
    buffer[0] = '\r';
    ++buffer;
    buffer[0] = '\n';
    ++buffer;
  }
  buffer[0] = '\r';
  ++buffer;
  buffer[0] = '\n';
  ++buffer;
  if(message->body != NULL) {
    (void) memcpy(buffer, message->body, message->body_len);
  }
}


#define ATLEAST(a) \
do { \
  if((a) > size) return http_incomplete; \
} while(0)

static int http1_parse_headers_and_body(char* buffer, uint32_t size, struct http_parser_session* const session,
  const struct http_parser_settings* const settings, struct http_message* const message) {
  uint32_t idx = 0;
  if(session->last_at == http_pla_body) {
    goto pla_body;
  }
  ATLEAST(2);
  if(settings->max_headers == 0) {
    if(buffer[0] == '\r') {
      session->last_idx += 2;
      buffer += 2;
      size -= 2;
    } else {
      return http_has_headers_but_max_is_0;
    }
  } else {
    uint32_t i = session->last_header_idx;
    for(; i < settings->max_headers; ++i) {
      if(buffer[0] == '\r') {
        session->last_idx += 2;
        buffer += 2;
        size -= 2;
        break;
      }
      /* Header name */
      ATLEAST(5);
      if(!settings->no_character_validation) {
        uint32_t j = 0;
        for(; j < size; ++j) {
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
          session->last_header_idx = i;
          return http_incomplete;
        }
        if(j == 0) {
          return http_no_header_name;
        }
        message->headers[i].name = buffer;
        message->headers[i].name_len = j;
        idx += j + 1;
      } else {
        const char* const colon = memchr(buffer, ':', size);
        if(colon == NULL) {
          if(size > settings->max_header_name_len) {
            return http_header_name_too_long;
          }
          session->last_header_idx = i;
          return http_incomplete;
        }
        const uint32_t header_name_len = (uint32_t)((uintptr_t) colon - (uintptr_t) buffer);
        if(header_name_len > settings->max_header_name_len) {
          return http_header_name_too_long;
        }
        if(header_name_len == 0) {
          return http_no_header_name;
        }
        message->headers[i].name = buffer;
        message->headers[i].name_len = header_name_len;
        idx += header_name_len + 1;
      }
      size -= idx;
      ATLEAST(3);
      buffer += idx + 1;
      --size;
      uint32_t whole_idx = idx + 1;
      idx = 0;
      /* Header value */
      if(!settings->no_character_validation) {
        uint32_t j = 0;
        for(; j < size; ++j) {
          if(!IS_TOKEN(buffer[j])) {
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
          session->last_header_idx = i;
          return http_incomplete;
        }
        message->headers[i].value = buffer;
        message->headers[i].value_len = j;
        idx += j + 1;
      } else {
        const char* const cr = memchr(buffer, '\r', size);
        if(cr == NULL) {
          if(size > settings->max_header_value_len) {
            return http_header_value_too_long;
          }
          session->last_header_idx = i;
          return http_incomplete;
        }
        const uint32_t header_value_len = (uint32_t)((uintptr_t) cr - (uintptr_t) buffer);
        if(header_value_len > settings->max_header_value_len) {
          return http_header_value_too_long;
        }
        /* Header value length can be 0, so no check for that */
        message->headers[i].value = buffer;
        message->headers[i].value_len = header_value_len;
        idx += header_value_len + 1;
      }
      if(!settings->no_body_parsing) {
        if(message->headers[i].name_len == 14 && strncasecmp(message->headers[i].name, "content-length", 14) == 0) {
          if(message->headers[i].value_len > 10) {
            /* Certainly bigger than maximum int value */
            return http_body_too_long;
          }
          char str[11];
          (void) memcpy(str, message->headers[i].value, 10);
          str[10] = 0;
          const uint32_t body_len = atoi(str);
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
          if(message->headers[i].value_len == 7 && strncasecmp(message->headers[i].value, "deflate", 7) == 0) {
            message->encoding = http_e_deflate;
          } else if(message->headers[i].value_len == 2 && strncasecmp(message->headers[i].value, "br", 2) == 0) {
            message->encoding = http_e_brotli;
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
      idx = 0;
      session->last_idx += whole_idx;
      session->last_header_idx = i + 1;
      if(settings->stop_at_every_header) {
        return http_valid;
      }
    }
    if(i == settings->max_headers) {
      return http_too_many_headers;
    }
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
    decode:
    if(message->allocated_body) {
      buffer = message->body;
    }
    switch(message->encoding) {
      case http_e_none: {
        message->body = buffer;
        break;
      }
      case http_e_deflate: {
        size_t len;
        errno = 0;
        message->body = deflate_decompress(buffer, message->body_len, &len, settings->max_body_len);
        if(message->body == NULL) {
          if(errno == ENOMEM) {
            return http_out_of_memory;
          } else if(errno == EOVERFLOW) {
            return http_body_too_long;
          } else {
            return http_corrupted_body_compression;
          }
        }
        message->body_len = len;
        if(message->allocated_body) {
          free(buffer);
        }
        message->allocated_body = 1;
        break;
      }
      case http_e_brotli: {
        size_t len;
        message->body = brotli_decompress(buffer, message->body_len, &len, settings->max_body_len);
        if(message->body == NULL) {
          if(errno == ENOMEM) {
            return http_out_of_memory;
          } else if(errno == EOVERFLOW) {
            return http_body_too_long;
          } else {
            return http_corrupted_body_compression;
          }
        }
        message->body_len = len;
        if(message->allocated_body) {
          free(buffer);
        }
        message->allocated_body = 1;
        break;
      }
    }
    /* No encoding now */
    message->encoding = http_e_none;
  } else {
    while(1) {
      uint32_t chunk_len = 0;
      uint32_t i = 0;
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
      if(chunk_len == 0) {
        session->last_idx += i + 2;
        if(message->body_len > 0) {
          /* So that if we resume the session, we won't end up here */
          message->transfer = http_t_none;
          goto decode;
        }
      }
      ATLEAST(chunk_len + 2);
      char* const ptr = realloc(message->body, message->body_len + chunk_len);
      if(ptr == NULL) {
        return http_out_of_memory;
      }
      message->allocated_body = 1;
      message->body = ptr;
      (void) memcpy(message->body + message->body_len, buffer, chunk_len);
      message->body_len += chunk_len;
      session->last_idx += i + 4 + chunk_len;
      buffer += chunk_len + 2;
      size -= chunk_len + 2;
    }
  }
  session->parsed_body = 1;
  return http_valid;
}

/* message->headers and message->headers_len must be initialised */

int http1_parse_request(char* buffer, uint32_t size, struct http_parser_session* const session,
  const struct http_parser_settings* const settings, struct http_message* const message) {
  uint32_t idx = 0;
  buffer += session->last_idx;
  size -= session->last_idx;
  switch(session->last_at) {
    case http_pla_method: break;
    case http_pla_path: goto pla_path;
    case http_pla_version: goto pla_version;
    case http_pla_body:
    case http_pla_headers: goto pla_headers;
  }
  ATLEAST(2);
  if(!settings->no_character_validation) {
    uint32_t i = 0;
    for(; i < size; ++i) {
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
    message->method = buffer;
    message->method_len = i;
    idx += i + 1;
  } else {
    const char* const space = memchr(buffer, ' ', size);
    if(space == NULL) {
      if(size > settings->max_method_len) {
        return http_method_too_long;
      }
      return http_incomplete;
    }
    const uint32_t method_len = (uint32_t)((uintptr_t) space - (uintptr_t) buffer);
    if(method_len > settings->max_method_len) {
      return http_method_too_long;
    }
    if(method_len == 0) {
      return http_no_method;
    }
    message->method = buffer;
    message->method_len = method_len;
    idx += method_len + 1;
  }
  session->last_at = http_pla_path;
  session->parsed_method = 1;
  session->last_idx += idx;
  if(settings->stop_at_method) {
    return http_valid;
  }
  buffer += idx;
  size -= idx;
  idx = 0;
  pla_path:
  /* Don't validate the path here, let the user choose what to do with it. That's
  because some filesystems have reserved characters for filenames, others don't. */
  {
    const char* const space = memchr(buffer, ' ', size);
    if(space == NULL) {
      if(size > settings->max_path_len) {
        return http_path_too_long;
      }
      return http_incomplete;
    }
    const uint32_t path_len = (uint32_t)((uintptr_t) space - (uintptr_t) buffer);
    if(path_len > settings->max_path_len) {
      return http_path_too_long;
    }
    if(path_len == 0) {
      return http_no_path;
    }
    message->path = buffer;
    message->path_len = path_len;
    idx += path_len + 1;
  }
  session->last_at = http_pla_version;
  session->parsed_path = 1;
  session->last_idx += idx;
  if(settings->stop_at_path) {
    return http_valid;
  }
  buffer += idx;
  size -= idx;
  idx = 0;
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

int http1_parse_response(char* buffer, uint32_t size, struct http_parser_session* const session,
  const struct http_parser_settings* const settings, struct http_message* const message) {
  uint32_t idx = 0;
  buffer += session->last_idx;
  size -= session->last_idx;
  switch(session->last_at) {
    case http_pla_res_version: break;
    case http_pla_status_code: goto pla_status_code;
    case http_pla_reason_phrase: goto pla_reason_phrase;
    case http_pla_body:
    case http_pla_headers: goto pla_headers;
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
  ATLEAST(4);
  if(!IS_DIGIT(buffer[0]) || !IS_DIGIT(buffer[1]) || !IS_DIGIT(buffer[2])) {
    return http_invalid_character;
  }
  message->status_code = (buffer[0] - 48) * 100 + (buffer[1] - 48) * 10 + buffer[2] - 48;
  if(message->status_code < 100 || message->status_code > 999) {
    return http_invalid_status_code;
  }
  session->last_at = http_pla_reason_phrase;
  session->parsed_status_code = 1;
  session->last_idx += 4;
  if(settings->stop_at_status_code) {
    return http_valid;
  }
  buffer += 4;
  size -= 4;
  pla_reason_phrase:
  ATLEAST(2);
  if(!settings->no_character_validation) {
    uint32_t i = 0;
    for(; i < size; ++i) {
      if(buffer[i] == '\r' || buffer[i] < 32 || buffer[i] > 126 || buffer[i] == '\n') {
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
    const char* const cr = memchr(buffer, '\r', size);
    if(cr == NULL) {
      if(size > settings->max_reason_phrase_len) {
        return http_reason_phrase_too_long;
      }
      return http_incomplete;
    }
    const uint32_t reason_phrase_len = (uint32_t)((uintptr_t) cr - (uintptr_t) buffer);
    if(reason_phrase_len > settings->max_reason_phrase_len) {
      return http_reason_phrase_too_long;
    }
    message->reason_phrase = buffer;
    message->reason_phrase_len = reason_phrase_len;
    idx = reason_phrase_len + 1;
  }
  size -= idx;
  ATLEAST(1);
  buffer += idx + 1;
  --size;
  session->last_at = http_pla_headers;
  session->parsed_reason_phrase = 1;
  session->last_idx += idx + 1;
  if(settings->stop_at_reason_phrase) {
    return http_valid;
  }
  pla_headers:
  return http1_parse_headers_and_body(buffer, size, session, settings, message);
}

#undef ATLEAST

struct http_header* http1_seek_header(const struct http_message* const message, const char* const name, const uint32_t len) {
  for(uint32_t i = 0; i < message->headers_len; ++i) {
    if(message->headers[i].name_len == len && strncasecmp(message->headers[i].name, name, len) == 0) {
      return message->headers + i;
    }
  }
  return NULL;
}