empty: ;
.PHONY: empty build test clean prepare include copy-headers build-static static strip-static build-dynamic dynamic strip-dynamic uninstall

CMPE_CORE=gcc -pthread -Wall -pedantic
CMPE=O3
ifneq ($(debug),)
COMP=$(CMPE_CORE) -$(CMPE) -D SHNET_DEBUG
else
COMP=$(CMPE_CORE) -$(CMPE)
endif
FILENAMES=build/debug.o build/heap.o build/refheap.o build/avl.o build/threads.o build/contmem.o build/time.o build/net.o build/udp.o build/udplite.o build/aflags.o build/tcp.o build/tls.o build/compress.o build/http_p.o

build: prepare $(FILENAMES)

test: $(wildcard tests/*.c) $(wildcard tests/*.h)
	

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
	ar rcsv build/libshnet.a $(FILENAMES)

static: build-static include
	cp build/libshnet.a /usr/lib/
	cp build/libshnet.a /usr/local/lib/

strip-static: build-static copy-headers
	cp build/libshnet.a shnet/

build-dynamic: build
	$(COMP) $(FILENAMES) -shared -o build/libshnet.so -lssl -lcrypto -lz -lbrotlienc -lbrotlidec

dynamic: build-dynamic include
	cp build/libshnet.so /usr/lib/
	cp build/libshnet.so /usr/local/lib/

strip-dynamic: build-dynamic copy-headers
	cp build/libshnet.so shnet/

uninstall:
	rm -f /usr/lib/libshnet.* /usr/local/lib/libshnet.*
	rm -r -f /usr/include/shnet /usr/local/include/shnet shnet


build/debug.o: src/debug.h src/debug.c
	$(COMP) -fPIC -c src/debug.c -o build/debug.o

build/heap.o: src/heap.h src/heap.c
	$(COMP) -fPIC -c src/heap.c -o build/heap.o

build/refheap.o: src/refheap.h src/refheap.c build/heap.o
	$(COMP) -fPIC -c src/refheap.c -o build/refheap.o

build/avl.o: src/avl.h src/avl.c
	$(COMP) -fPIC -c src/avl.c -o build/avl.o

build/threads.o: src/threads.h src/threads.c
	$(COMP) -fPIC -c src/threads.c -o build/threads.o

build/contmem.o: src/contmem.h src/contmem.c
	$(COMP) -fPIC -c src/contmem.c -o build/contmem.o

build/time.o: src/time.h src/time.c build/refheap.o build/threads.o
	$(COMP) -fPIC -c src/time.c -o build/time.o

build/net.o: src/net.h src/net.c build/threads.o
	$(COMP) -fPIC -c src/net.c -o build/net.o

build/udp.o: src/udp.h src/udp.c build/net.o
	$(COMP) -fPIC -c src/udp.c -o build/udp.o

build/udplite.o: src/udplite.h src/udplite.c build/udp.o
	$(COMP) -fPIC -c src/udplite.c -o build/udplite.o

build/aflags.o: src/aflags.h src/aflags.c
	$(COMP) -fPIC -c src/aflags.c -o build/aflags.o

build/tcp.o: src/tcp.h src/tcp.c build/net.o
	$(COMP) -fPIC -c src/tcp.c -o build/tcp.o

build/tls.o: src/tls.h src/tls.c build/tcp.o
	$(COMP) -fPIC -c src/tls.c -o build/tls.o

build/compress.o: src/compress.h src/compress.c
	$(COMP) -fPIC -c src/compress.c -o build/compress.o

build/http_p.o: src/http_p.h src/http_p.c
	$(COMP) -fPIC -c src/http_p.c -o build/http_p.o
