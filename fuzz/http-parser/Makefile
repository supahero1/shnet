.PHONY: all fuzz

all: fuzz

fuzz:
	rm -f fuzz
	afl-clang-fast -Wall -pedantic -g -w -O3 fuzz.c ../../src/http_p.c ../../src/compress.c -o fuzz -lz -lbrotlienc -lbrotlidec