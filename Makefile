empty: ;
.PHONY: empty build test build-tests clean prepare include copy-headers build-static static strip-static build-dynamic dynamic strip-dynamic uninstall

CC = gcc
CFLAGS += -pthread -Wall -pedantic -O3
LDLIBS += -lshnet
TESTLIBS = -lm -lssl -lcrypto -lz -lbrotlienc -lbrotlidec

ifneq ($(debug),)
CFLAGS += -D SHNET_DEBUG
endif

SOURCES = $(wildcard src/*.c)
OBJECTS = $(SOURCES:src/%.c=build/%.o)

TEST_SUITES = $(wildcard tests/*.c)
TEST_EXECS = $(TEST_SUITES:tests/%.c=build/test_%)

build: prepare $(OBJECTS)

test: prepare $(TEST_EXECS)
	build/test_heap
	build/test_avl
	build/test_threads
	build/test_time
	build/test_udp
	
	build/test_tcp 1 1 1
	build/test_tcp 1 1 0
	build/test_tcp 100 1 1
	build/test_tcp 100 1 0
	build/test_tcp 100 100 1
	build/test_tcp 100 100 0
	build/test_tcp 100 1000 1
	build/test_tcp 100 1000 0
	
	build/test_tcp_stress 1 1 1
	build/test_tcp_stress 1 1 0
	build/test_tcp_stress 1 10 1
	build/test_tcp_stress 1 10 0
	build/test_tcp_stress 1 100 1
	build/test_tcp_stress 1 100 0
	build/test_tcp_stress 1 1000 1
	build/test_tcp_stress 1 1000 0
	
	build/test_tcp_stress 100 1 1
	build/test_tcp_stress 100 1 0
	build/test_tcp_stress 100 10 1
	build/test_tcp_stress 100 10 0
	build/test_tcp_stress 100 100 1
	build/test_tcp_stress 100 100 0
	build/test_tcp_stress 100 1000 1
	build/test_tcp_stress 100 1000 0
	
	build/test_tls 1 1 1
	build/test_tls 1 1 0
	build/test_tls 100 1 1
	build/test_tls 100 1 0
	build/test_tls 100 100 1
	build/test_tls 100 100 0
	build/test_tls 100 1000 1
	build/test_tls 100 1000 0
	
	build/test_tls_stress 1 1 1
	build/test_tls_stress 1 1 0
	build/test_tls_stress 1 10 1
	build/test_tls_stress 1 10 0
	build/test_tls_stress 1 100 1
	build/test_tls_stress 1 100 0
	build/test_tls_stress 1 1000 1
	build/test_tls_stress 1 1000 0
	
	build/test_tls_stress 100 1 1
	build/test_tls_stress 100 1 0
	build/test_tls_stress 100 10 1
	build/test_tls_stress 100 10 0
	build/test_tls_stress 100 100 1
	build/test_tls_stress 100 100 0
	build/test_tls_stress 100 1000 1
	build/test_tls_stress 100 1000 0
	
	build/test_compression
	build/test_http_p

build/test_%: tests/%.c tests/tests.h src/debug.c src/debug.h
	$(CC) $(CFLAGS) $< -o $@ $(TESTLIBS) $(LDLIBS)

clean:
	rm -r -f build logs.txt

prepare:
	mkdir -p build

include:
	mkdir -p /usr/include/shnet
	cp src/*.h /usr/include/shnet/
	mkdir -p /usr/local/include/shnet
	cp src/*.h /usr/local/include/shnet/

copy-headers:
	mkdir -p shnet
	cp src/*.h shnet/

build-static: build
	ar rcsv build/libshnet.a $(OBJECTS)

static: build-static include
	cp build/libshnet.a /usr/lib/
	cp build/libshnet.a /usr/local/lib/

strip-static: build-static copy-headers
	cp build/libshnet.a shnet/

build-dynamic: build
	$(CC) $(CFLAGS) $(OBJECTS) -shared -o build/libshnet.so -lssl -lcrypto -lz -lbrotlienc -lbrotlidec

dynamic: build-dynamic include
	cp build/libshnet.so /usr/lib/
	cp build/libshnet.so /usr/local/lib/

strip-dynamic: build-dynamic copy-headers
	cp build/libshnet.so shnet/

uninstall:
	rm -f /usr/lib/libshnet.* /usr/local/lib/libshnet.*
	rm -r -f /usr/include/shnet /usr/local/include/shnet shnet


build/refheap.o: build/heap.o

build/time.o: build/threads.o

build/net.o: build/threads.o

build/udp.o: build/net.o

build/udplite.o: build/udp.o

build/tcp.o: build/net.o

build/tls.o: build/tcp.o

build/%.o: src/%.c src/%.h
	$(CC) $(CFLAGS) -fPIC -c $< -o $@