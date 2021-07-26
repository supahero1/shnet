#include "base64.h"

#include <stddef.h>
#include <stdlib.h>
#include <stdint.h>

static char enc[] = {
   65,  66,  67,  68,  69,  70,  71,  72,
   73,  74,  75,  76,  77,  78,  79,  80,
   81,  82,  83,  84,  85,  86,  87,  88,
   89,  90,  97,  98,  99, 100, 101, 102,
  103, 104, 105, 106, 107, 108, 109, 110,
  111, 112, 113, 114, 115, 116, 117, 118,
  119, 120, 121, 122,  48,  49,  50,  51,
   52,  53,  54,  55,  56,  57,  43,  47
};

static char dec[] = {
   0,  0,  0,  0,  0,  0,  0,  0,
   0,  0,  0,  0,  0,  0,  0,  0,
   0,  0,  0,  0,  0,  0,  0,  0,
   0,  0,  0,  0,  0,  0,  0,  0,
   0,  0,  0,  0,  0,  0,  0,  0,
   0,  0,  0, 62,  0,  0,  0, 63,
  52, 53, 54, 55, 56, 57, 58, 59,
  60, 61,  0,  0,  0,  0,  0,  0,
   0,  0,  1,  2,  3,  4,  5,  6,
   7,  8,  9, 10, 11, 12, 13, 14,
  15, 16, 17, 18, 19, 20, 21, 22,
  23, 24, 25,  0,  0,  0,  0,  0,
   0, 26, 27, 28, 29, 30, 31, 32,
  33, 34, 35, 36, 37, 38, 39, 40,
  41, 42, 43, 44, 45, 46, 47, 48,
  49, 50, 51
};

uint64_t base64_encoded_len(uint64_t size) {
  return (((size + 2) / 3) << 2) + 1;
}

uint64_t base64_decoded_len(const uint64_t size) {
  return ((size * 3) >> 2) + 1;
}

void* base64_encode(const void* _in, void* _out, const uint64_t size) {
  const unsigned char* in = _in;
  char* out = _out;
  const uint64_t len = base64_encoded_len(size);
  if(out == NULL) {
    out = malloc(len);
    if(out == NULL) {
      return NULL;
    }
  }
  const uint64_t full_encodes = (size - (size % 3)) / 3;
  /* Fast loop */
  for(uint64_t i = 0; i < full_encodes; ++i) {
    *out = enc[in[0] >> 2];
    ++out;
    *out = enc[((in[0] & 3) << 4) | (in[1] >> 4)];
    ++out;
    ++in;
    *out = enc[((in[0] & 15) << 2) | (in[1] >> 6)];
    ++out;
    ++in;
    *out = enc[in[0] & 63];
    ++out;
    ++in;
  }
  /* The last bytes */
  switch(size % 3) {
    case 0: break;
    case 1: {
      *out = enc[in[0] >> 2];
      ++out;
      *out = enc[(in[0] & 3) << 4];
      ++out;
      *out = '=';
      ++out;
      *out = '=';
      ++out;
      break;
    }
    case 2: {
      *out = enc[in[0] >> 2];
      ++out;
      *out = enc[((in[0] & 3) << 4) | (in[1] >> 4)];
      ++out;
      ++in;
      *out = enc[(in[0] & 15) << 2];
      ++out;
      *out = '=';
      ++out;
      break;
    }
    default: __builtin_unreachable();
  }
  *out = 0;
  ++out;
  return out - len;
}

void* base64_decode(const void* _in, void* _out, const uint64_t size) {
  const char* in = _in;
  unsigned char* out = _out;
  const uint64_t len = base64_decoded_len(size);
  if(out == NULL) {
    out = malloc(len);
    if(out == NULL) {
      return NULL;
    }
  }
  const uint64_t full_decodes = (size >> 2) - 1;
  /* Fast loop */
  for(uint64_t i = 0; i < full_decodes; ++i) {
    *out = (dec[(uint8_t) in[0]] << 2) | (dec[(uint8_t) in[1]] >> 4);
    ++out;
    ++in;
		*out = (dec[(uint8_t) in[0]] << 4) | (dec[(uint8_t) in[1]] >> 2);
    ++out;
    ++in;
		*out = (dec[(uint8_t) in[0]] << 6) | dec[(uint8_t) in[1]];
    ++out;
    in += 2;
  }
  /* The last 4 bytes */
  *out = dec[(uint8_t) in[0]] << 2;
  ++in;
  if(in[0] != '=') {
    *out |= dec[(uint8_t) in[0]] >> 4;
    ++out;
    *out = dec[(uint8_t) in[0]] << 4;
    ++in;
    if(in[0] != '=') {
      *out |= dec[(uint8_t) in[0]] >> 2;
      ++out;
      *out = dec[(uint8_t) in[0]] << 6;
      ++in;
      if(in[0] != '=') {
        *out = dec[(uint8_t) in[0]];
      }
      ++out;
    } else {
      out += 2;
    }
  } else {
    out += 3;
  }
  *out = 0;
  ++out;
  return out - len;
}