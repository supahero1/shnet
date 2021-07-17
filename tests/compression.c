#include "tests.h"

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <shnet/compress.h>

void brotli_test(char* text) {
  size_t len = strlen(text) + 1;
  size_t brotli_compressed_length;
  char* brotli_compressed = brotli_compress(text, len, &brotli_compressed_length);
  if(brotli_compressed == NULL) {
    TEST_FAIL;
  }
  size_t brotli_decompressed_length;
  char* brotli_decompressed = brotli_decompress(brotli_compressed, brotli_compressed_length, &brotli_decompressed_length, 8192);
  if(brotli_decompressed == NULL) {
    TEST_FAIL;
  }
  if(len != brotli_decompressed_length) {
    TEST_FAIL;
  }
  if(strcmp(brotli_decompressed, text) != 0) {
    TEST_FAIL;
  }
  free(brotli_compressed);
  free(brotli_decompressed);
}

void deflate_test(char* text) {
  size_t len = strlen(text) + 1;
  size_t deflate_compressed_length;
  char* deflate_compressed = deflate_compress(text, len, &deflate_compressed_length);
  if(deflate_compressed == NULL) {
    TEST_FAIL;
  }
  size_t deflate_decompressed_length;
  char* deflate_decompressed = deflate_decompress(deflate_compressed, deflate_compressed_length, &deflate_decompressed_length, 8192);
  if(deflate_decompressed == NULL) {
    TEST_FAIL;
  }
  if(len != deflate_decompressed_length) {
    TEST_FAIL;
  }
  if(strcmp(deflate_decompressed, text) != 0) {
    TEST_FAIL;
  }
  free(deflate_compressed);
  free(deflate_decompressed);
}

void gzip_test(char* text) {
  size_t len = strlen(text) + 1;
  size_t gzip_compressed_length;
  char* gzip_compressed = gzip_compress(text, len, &gzip_compressed_length);
  if(gzip_compressed == NULL) {
    TEST_FAIL;
  }
  size_t gzip_decompressed_length;
  char* gzip_decompressed = gzip_decompress(gzip_compressed, gzip_compressed_length, &gzip_decompressed_length, 8192);
  if(gzip_decompressed == NULL) {
    TEST_FAIL;
  }
  if(len != gzip_decompressed_length) {
    TEST_FAIL;
  }
  if(strcmp(gzip_decompressed, text) != 0) {
    TEST_FAIL;
  }
  free(gzip_compressed);
  free(gzip_decompressed);
}

int main() {
  _debug("Testing compression:", 1);
  
  puts("Test suite 1");
  brotli_test("L");
  puts("Test suite 2");
  brotli_test("Lorem ipsum dolor sit amet");
  puts("Test suite 3");
  brotli_test("Lorem ipsum dolor sit amet, consectetur adipiscing elit, sed do eiusmod tempor incididunt ut labore et dolore magna aliqua. Ut enim ad minim veniam, quis nostrud exercitation ullamco laboris nisi ut aliquip ex ea commodo consequat.");
  puts("Test suite 4");
  brotli_test("Lorem ipsum dolor sit amet, consectetur adipiscing elit, sed do eiusmod tempor incididunt ut labore et dolore magna aliqua. Ut enim ad minim veniam, quis nostrud exercitation ullamco laboris nisi ut aliquip ex ea commodo consequat. Duis aute irure dolor in reprehenderit in voluptate velit esse cillum dolore eu fugiat nulla pariatur. Excepteur sint occaecat cupidatat non proident, sunt in culpa qui officia deserunt mollit anim id est laborum.");
  
  puts("Test suite 5");
  deflate_test("L");
  puts("Test suite 6");
  deflate_test("Lorem ipsum dolor sit amet");
  puts("Test suite 7");
  deflate_test("Lorem ipsum dolor sit amet, consectetur adipiscing elit, sed do eiusmod tempor incididunt ut labore et dolore magna aliqua. Ut enim ad minim veniam, quis nostrud exercitation ullamco laboris nisi ut aliquip ex ea commodo consequat.");
  puts("Test suite 8");
  deflate_test("Lorem ipsum dolor sit amet, consectetur adipiscing elit, sed do eiusmod tempor incididunt ut labore et dolore magna aliqua. Ut enim ad minim veniam, quis nostrud exercitation ullamco laboris nisi ut aliquip ex ea commodo consequat. Duis aute irure dolor in reprehenderit in voluptate velit esse cillum dolore eu fugiat nulla pariatur. Excepteur sint occaecat cupidatat non proident, sunt in culpa qui officia deserunt mollit anim id est laborum.");
  
  puts("Test suite 9");
  gzip_test("L");
  puts("Test suite 10");
  gzip_test("Lorem ipsum dolor sit amet");
  puts("Test suite 11");
  gzip_test("Lorem ipsum dolor sit amet, consectetur adipiscing elit, sed do eiusmod tempor incididunt ut labore et dolore magna aliqua. Ut enim ad minim veniam, quis nostrud exercitation ullamco laboris nisi ut aliquip ex ea commodo consequat.");
  puts("Test suite 12");
  gzip_test("Lorem ipsum dolor sit amet, consectetur adipiscing elit, sed do eiusmod tempor incididunt ut labore et dolore magna aliqua. Ut enim ad minim veniam, quis nostrud exercitation ullamco laboris nisi ut aliquip ex ea commodo consequat. Duis aute irure dolor in reprehenderit in voluptate velit esse cillum dolore eu fugiat nulla pariatur. Excepteur sint occaecat cupidatat non proident, sunt in culpa qui officia deserunt mollit anim id est laborum.");
  
  TEST_PASS;
  return 0;
}