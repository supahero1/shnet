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

z_stream* deflater(z_stream* stream, const int level, const int window_bits, const int mem_level, const int strategy) {
  uint8_t alloc = 0;
  if(stream == NULL) {
    alloc = 1;
    stream = calloc(1, sizeof(z_stream));
    if(stream == NULL) {
      return NULL;
    }
  }
  if(deflateInit2(stream, level, Z_DEFLATED, window_bits, mem_level, strategy) != Z_OK) {
    if(alloc) {
      free(stream);
    }
    return NULL;
  }
  return stream;
}

void* deflate_(z_stream* stream, void* const input, const size_t input_len, void* output, size_t* output_len, int flush) {
  uint8_t alloc_s = 0;
  z_stream s;
  if(stream == NULL) {
    /* Assume no context takeover, just 1 compression */
    alloc_s = 1;
    s = (z_stream) {0};
    stream = deflater(&s, Z_DEFAULT_COMPRESSION, Z_DEFLATE_DEFAULT_WINDOW_BITS, Z_DEFAULT_MEM_LEVEL, Z_DEFAULT_STRATEGY);
    if(stream == NULL) {
      return NULL;
    }
    flush = Z_FINISH;
  }
  size_t len;
  if(output_len == NULL) {
    output_len = &len;
    len = deflateBound(stream, input_len);
  } else if(*output_len == 0) {
    *output_len = deflateBound(stream, input_len);
  }
  uint8_t alloc_o = 0;
  if(output == NULL) {
    alloc_o = 1;
    output = malloc(*output_len);
    if(output == NULL) {
      if(alloc_s) {
        deflateEnd(stream);
      }
      return NULL;
    }
  }
  stream->next_in = input;
  stream->avail_in = input_len;
  stream->next_out = output;
  stream->avail_out = *output_len;
  switch(deflate(stream, flush)) {
    case Z_OK:
    case Z_STREAM_END: {
      *output_len -= stream->avail_out;
      if(alloc_s) {
        deflateEnd(stream);
      }
      return output;
    }
    default: {
      if(alloc_o) {
        free(output);
      }
      if(alloc_s) {
        deflateEnd(stream);
      }
      errno = EINVAL;
      return NULL;
    }
  }
}

void deflater_free(z_stream* const stream) {
  deflateEnd(stream);
  free(stream);
}

z_stream* inflater(z_stream* stream, const int window_bits) {
  uint8_t alloc = 0;
  if(stream == NULL) {
    alloc = 1;
    stream = calloc(1, sizeof(z_stream));
    if(stream == NULL) {
      return NULL;
    }
  }
  if(inflateInit2(stream, window_bits) != Z_OK) {
    if(alloc) {
      free(stream);
    }
    return NULL;
  }
  return stream;
}

void* inflate_(z_stream* stream, void* const input, size_t input_len, void* output, size_t* output_len, const size_t limit, int flush) {
  size_t avail = input_len << 1;
  uint8_t alloc_o = 0;
  if(output == NULL) {
    alloc_o = 1;
    output = malloc(avail);
    if(output == NULL) {
      return NULL;
    }
  }
  z_stream s;
  uint8_t origin_null = 0;
  if(stream == NULL) {
    origin_null = 1;
    s = (z_stream) {0};
    stream = &s;
  }
  stream->next_in = input;
  stream->avail_in = input_len;
  uint8_t alloc_s = 0;
  if(origin_null) {
    /* Assume no context takeover, just 1 decompression */
    alloc_s = 1;
    stream = inflater(stream, Z_DEFLATE_DEFAULT_WINDOW_BITS);
    if(stream == NULL) {
      if(alloc_o) {
        free(output);
      }
      return NULL;
    }
    flush = Z_NO_FLUSH;
  }
  stream->next_out = output;
  stream->avail_out = avail;
  input_len <<= 1;
  size_t total = 0;
  while(1) {
    switch(inflate(stream, flush)) {
      case Z_OK: {
        if(stream->avail_out == 0 && stream->avail_in != 0) {
          total = input_len;
          avail = input_len;
          stream->avail_out += input_len;
          input_len <<= 1;
          if(input_len > limit) {
            if(alloc_o) {
              free(output);
            }
            if(alloc_s) {
              inflateEnd(stream);
            }
            errno = EOVERFLOW;
            return NULL;
          }
          output = realloc(output, input_len);
          if(output == NULL) {
            if(alloc_s) {
              inflateEnd(stream);
            }
            return NULL;
          }
          stream->next_out = (unsigned char*) output + avail;
          break;
        }
        if(stream->avail_in == 0) {
          goto stream_end;
        } /* Else, strangely, there are more blocks after the last one. Fail. */
      }
      default: {
        if(alloc_o) {
          free(output);
        }
        if(alloc_s) {
          inflateEnd(stream);
        }
        errno = EINVAL;
        return NULL;
      }
      case Z_STREAM_END: {
        stream_end:
        total += avail - stream->avail_out;
        if(output_len != NULL) {
          *output_len = total;
        }
        if(alloc_s) {
          inflateEnd(stream);
        }
        return output;
      }
      case Z_MEM_ERROR: {
        if(alloc_o) {
          free(output);
        }
        if(alloc_s) {
          inflateEnd(stream);
        }
        errno = ENOMEM;
        return NULL;
      }
      case Z_DATA_ERROR: {
        if(alloc_o) {
          free(output);
        }
        if(alloc_s) {
          inflateEnd(stream);
        }
        errno = EINVAL;
        return NULL;
      }
    }
  }
}

void inflater_free(z_stream* const stream) {
  inflateEnd(stream);
  free(stream);
}



z_stream* gzipper(z_stream* stream, const int level, const int window_bits, const int mem_level, const int strategy) {
  return deflater(stream, level, window_bits | 16, mem_level, strategy);
}

void* gzip(z_stream* stream, void* const input, const size_t input_len, void* output, size_t* output_len, int flush) {
  uint8_t alloc_s = 0;
  z_stream s;
  if(stream == NULL) {
    alloc_s = 1;
    s = (z_stream) {0};
    stream = gzipper(&s, Z_DEFAULT_COMPRESSION, Z_GZIP_DEFAULT_WINDOW_BITS, Z_DEFAULT_MEM_LEVEL, Z_DEFAULT_STRATEGY);
    if(stream == NULL) {
      return NULL;
    }
    flush = Z_FINISH;
  }
  void* const ret = deflate_(stream, input, input_len, output, output_len, flush);
  if(alloc_s) {
    deflateEnd(stream);
  }
  return ret;
}

void gzipper_free(z_stream* const stream) {
  deflater_free(stream);
}

z_stream* gunzipper(z_stream* stream, const int window_bits) {
  return inflater(stream, window_bits | 16);
}

void* gunzip(z_stream* stream, void* const input, size_t input_len, void* output, size_t* output_len, const size_t limit, int flush) {
  uint8_t alloc_s = 0;
  z_stream s;
  if(stream == NULL) {
    alloc_s = 1;
    s = (z_stream) {0};
    stream = gunzipper(&s, Z_GZIP_DEFAULT_WINDOW_BITS);
    if(stream == NULL) {
      return NULL;
    }
    flush = Z_NO_FLUSH;
  }
  void* const ret = inflate_(stream, input, input_len, output, output_len, limit, flush);
  if(alloc_s) {
    inflateEnd(stream);
  }
  return ret;
}

void gunzipper_free(z_stream* const stream) {
  inflater_free(stream);
}