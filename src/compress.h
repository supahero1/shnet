#ifndef EfOx_7R__HX1BItU5S_d__q_0uXr_KT_
#define EfOx_7R__HX1BItU5S_d__q_0uXr_KT_ 1

#include <stdlib.h>

#include <brotli/encode.h>
#include <brotli/decode.h>

extern void* brotli_compress(const void* const, const size_t, size_t* const);

extern void* brotli_compress2(const void* const, const size_t, size_t* const, const int, const int, const int);

extern void* brotli_decompress(void* const, size_t, size_t* const, const size_t);


#include <zlib.h>

#define Z_DEFLATE_DEFAULT_WINDOW_BITS 15
#define Z_DEFAULT_MEM_LEVEL 8

extern void* deflate_compress(void* const, const size_t, size_t* const);

extern void* deflate_compress2(void* const, const size_t, size_t* const, const int, const int, const int, const int);

extern void* deflate_decompress(void* const, size_t, size_t* const, const size_t);

extern void* deflate_decompress2(void* const, size_t, size_t* const, const size_t, const int);


#define Z_GZIP_DEFAULT_WINDOW_BITS 31

extern void* gzip_compress(void* const, const size_t, size_t* const);

extern void* gzip_compress2(void* const, const size_t, size_t* const, const int, const int, const int, const int);

extern void* gzip_decompress(void* const, size_t, size_t* const, const size_t);

extern void* gzip_decompress2(void* const, size_t, size_t* const, const size_t, const int);

#endif // EfOx_7R__HX1BItU5S_d__q_0uXr_KT_