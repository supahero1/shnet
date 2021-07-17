#include "compress.h"

#include <errno.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

/*
Quality:
from BROTLI_MIN_QUALITY to BROTLI_MAX_QUALITY, default is BROTLI_DEFAULT_QUALITY

Window size:
from BROTLI_MIN_WINDOW_BITS to BROTLI_MAX_WINDOW_BITS, default is BROTLI_DEFAULT_WINDOW

Mode:
BROTLI_MODE_GENERIC if input not known, otherwise BROTLI_MODE_TEXT (utf-8) or BROTLI_MODE_FONT (WOFF 2.0)
*/

void* brotli_compress2(const void* const input, const size_t input_len, size_t* const output_len,
  const int quality, const int window_size, const int mode) {
  *output_len = BrotliEncoderMaxCompressedSize(input_len);
  void* const output = malloc(*output_len);
  if(output == NULL) {
    return NULL;
  }
  int err = BrotliEncoderCompress(quality, window_size, mode, input_len, input, output_len, output);
  if(err == BROTLI_TRUE) {
    return output;
  }
  free(output);
  errno = EINVAL;
  return NULL;
}

void* brotli_compress(const void* const input, const size_t input_len, size_t* const output_len) {
  return brotli_compress2(input, input_len, output_len, BROTLI_DEFAULT_QUALITY, BROTLI_DEFAULT_WINDOW, BROTLI_MODE_GENERIC);
}

void* brotli_decompress(void* const input, size_t input_len, size_t* const output_len, const size_t limit) {
  BrotliDecoderState* state = BrotliDecoderCreateInstance(NULL, NULL, NULL);
  if(state == NULL) {
    return NULL;
  }
  void* output = malloc(input_len);
  if(output == NULL) {
    BrotliDecoderDestroyInstance(state);
    return NULL;
  }
  size_t available_in = input_len;
  size_t available_out = input_len;
  const void* input_bytes = input;
  void* output_bytes = output;
  size_t total;
  errno = 0;
  while(1) {
    switch(BrotliDecoderDecompressStream(state, &available_in, (const uint8_t**)&input_bytes, &available_out, (uint8_t**)&output_bytes, &total)) {
      case BROTLI_DECODER_RESULT_ERROR: {
        const int err = BrotliDecoderGetErrorCode(state);
        if(err >= BROTLI_DECODER_ERROR_ALLOC_BLOCK_TYPE_TREES && err <= BROTLI_DECODER_ERROR_ALLOC_CONTEXT_MODES) {
          errno = ENOMEM;
        } else {
          errno = EINVAL;
        }
        BrotliDecoderDestroyInstance(state);
        free(output);
        return NULL;
      }
      case BROTLI_DECODER_RESULT_NEEDS_MORE_INPUT: {
        BrotliDecoderDestroyInstance(state);
        free(output);
        errno = EINVAL;
        return NULL;
      }
      case BROTLI_DECODER_RESULT_NEEDS_MORE_OUTPUT: {
        available_out += input_len;
        input_len <<= 1;
        if(input_len > limit) {
          BrotliDecoderDestroyInstance(state);
          free(output);
          errno = EOVERFLOW;
          return NULL;
        }
        output = realloc(output, input_len);
        if(output == NULL) {
          BrotliDecoderDestroyInstance(state);
          return NULL;
        }
        output_bytes = (char*) output + total;
        break;
      }
      case BROTLI_DECODER_RESULT_SUCCESS: {
        BrotliDecoderDestroyInstance(state);
        *output_len = total;
        return output;
      }
    }
  }
}



/*
Levels:
Z_NO_COMPRESSION
Z_BEST_SPEED
Z_BEST_COMPRESSION
Z_DEFAULT_COMPRESSION

Window bits: anything from 9 to 15,
for deflate the default is Z_DEFLATE_DEFAULT_WINDOW_BITS
for  gzip   the default is Z_GZIP_DEFAULT_WINDOW_BITS

Mem level: anything from 1 to 9,
the default is Z_DEFAULT_MEM_LEVEL

Stategies:
Z_DEFAULT_STRATEGY
Z_FILTERED
Z_HUFFMAN_ONLY
Z_RLE
*/

void* deflate_compress2(void* const input, const size_t input_len, size_t* const output_len, const int level, const int window_bits, const int mem_level, const int strategy) {
  *output_len = compressBound(input_len);
  void* output;
  z_stream strm = {0};
  if(deflateInit2(&strm, level, Z_DEFLATED, window_bits, mem_level, strategy) != Z_OK) {
    errno = ENOMEM;
    return NULL;
  }
  *output_len = deflateBound(&strm, input_len);
  output = malloc(*output_len);
  if(output == NULL) {
    deflateEnd(&strm);
    return NULL;
  }
  strm.next_in = input;
  strm.avail_in = input_len;
  strm.next_out = output;
  strm.avail_out = *output_len;
  switch(deflate(&strm, Z_FINISH)) {
    case Z_STREAM_END: {
      deflateEnd(&strm);
      return output;
    }
    default: {
      free(output);
      deflateEnd(&strm);
      errno = EINVAL;
      return NULL;
    }
  }
}

void* deflate_compress(void* const input, const size_t input_len, size_t* const output_len) {
  *output_len = compressBound(input_len);
  void* const output = malloc(*output_len);
  if(output == NULL) {
    return NULL;
  }
  errno = compress2(output, output_len, input, input_len, Z_DEFAULT_COMPRESSION);
  if(errno != Z_OK) {
    free(output);
    errno = EINVAL;
    return NULL;
  }
  return output;
}

void* deflate_decompress2(void* const input, size_t input_len, size_t* const output_len, const size_t limit, const int window_bits) {
  void* output = malloc(input_len);
  if(output == NULL) {
    return NULL;
  }
  z_stream strm = {0};
  strm.next_in = input;
  strm.avail_in = input_len;
  strm.next_out = output;
  strm.avail_out = input_len;
  size_t avail = input_len;
  size_t total = 0;
  if(inflateInit2(&strm, window_bits) != Z_OK) {
    errno = ENOMEM;
    return NULL;
  }
  while(1) {
    switch(inflate(&strm, Z_NO_FLUSH)) {
      case Z_OK: {
        if(strm.avail_out == 0) {
          total = input_len;
          avail = input_len;
          strm.avail_out += input_len;
          input_len <<= 1;
          if(input_len > limit) {
            free(output);
            inflateEnd(&strm);
            errno = EOVERFLOW;
            return NULL;
          }
          output = realloc(output, input_len);
          if(output == NULL) {
            inflateEnd(&strm);
            return NULL;
          }
          strm.next_out = (unsigned char*) output + avail;
          break;
        }
        /* Input must be somehow invalid if we still have output space */
      }
      default: {
        free(output);
        inflateEnd(&strm);
        errno = EINVAL;
        return NULL;
      }
      case Z_STREAM_END: {
        total += avail - strm.avail_out;
        *output_len = total;
        inflateEnd(&strm);
        return output;
      }
      case Z_MEM_ERROR: {
        free(output);
        inflateEnd(&strm);
        errno = ENOMEM;
        return NULL;
      }
      case Z_DATA_ERROR: {
        free(output);
        inflateEnd(&strm);
        errno = EINVAL;
        return NULL;
      }
    }
  }
}

void* deflate_decompress(void* const input, size_t input_len, size_t* const output_len, const size_t limit) {
  return deflate_decompress2(input, input_len, output_len, limit, Z_DEFLATE_DEFAULT_WINDOW_BITS);
}



void* gzip_compress(void* const input, const size_t input_len, size_t* const output_len) {
  return deflate_compress2(input, input_len, output_len, Z_DEFAULT_COMPRESSION, Z_GZIP_DEFAULT_WINDOW_BITS, Z_DEFAULT_MEM_LEVEL, Z_DEFAULT_STRATEGY);
}

void* gzip_compress2(void* const input, const size_t input_len, size_t* const output_len, const int level, const int window_bits, const int mem_level, const int strategy) {
  return deflate_compress2(input, input_len, output_len, level, window_bits | 16, mem_level, strategy);
}

void* gzip_decompress(void* const input, size_t input_len, size_t* const output_len, const size_t limit) {
  return deflate_decompress2(input, input_len, output_len, limit, Z_GZIP_DEFAULT_WINDOW_BITS);
}

void* gzip_decompress2(void* const input, size_t input_len, size_t* const output_len, const size_t limit, const int window_bits) {
  return deflate_decompress2(input, input_len, output_len, limit, window_bits | 15);
}