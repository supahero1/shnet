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

extern z_stream* deflater(z_stream*, const int, const int, const int, const int);

extern void* deflate_(z_stream*, void* const, const size_t, void*, size_t*, int);

extern void deflater_free(z_stream* const);


extern z_stream* inflater(z_stream*, const int);

extern void* inflate_(z_stream*, void* const, size_t, void*, size_t*, const size_t, int);

extern void inflater_free(z_stream* const);


#define Z_GZIP_DEFAULT_WINDOW_BITS 31

extern z_stream* gzipper(z_stream*, const int, const int, const int, const int);

extern void* gzip(z_stream*, void* const, const size_t, void*, size_t*, int);

extern void gzipper_free(z_stream* const);


extern z_stream* gunzipper(z_stream*, const int);

extern void* gunzip(z_stream*, void* const, size_t, void*, size_t*, const size_t, int);

extern void gunzipper_free(z_stream* const);

#endif // EfOx_7R__HX1BItU5S_d__q_0uXr_KT_