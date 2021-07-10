#ifndef EfOx_7R__HX1BItU5S_d__q_0uXr_KT_
#define EfOx_7R__HX1BItU5S_d__q_0uXr_KT_ 1

#include <stdlib.h>

#include <brotli/encode.h>
#include <brotli/decode.h>

extern void* brotli_compress(const void* const, const size_t, size_t* const, const int, const int, const int);

extern void* brotli_decompress(void* const, size_t, size_t* const);


#include <zlib.h>

extern void* deflate_compress(void* const, const size_t, size_t* const, const int);

extern void* deflate_decompress(void* const, size_t, size_t* const);

#endif // EfOx_7R__HX1BItU5S_d__q_0uXr_KT_