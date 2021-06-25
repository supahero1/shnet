empty: ;
.PHONY: empty build test clean prepare include copy-headers build-static static strip-static build-dynamic dynamic strip-dynamic uninstall

CMPE_CORE=gcc -pthread -Wall -pedantic
CMPE=O3
ifneq ($(debug),)
COMP=$(CMPE_CORE) -$(CMPE) -D SHNET_DEBUG
else
COMP=$(CMPE_CORE) -$(CMPE)
endif
FILENAMES=build/debug.o build/heap.o build/refheap.o build/avl.o build/threads.o build/misc.o build/time.o build/net.o build/udp.o build/udplite.o build/aflags.o build/tcp.o

build: prepare $(FILENAMES)

test: $(wildcard tests/*.c) $(wildcard tests/*.h)
	$(COMP) tests/heap.c -o build/heap -lshnet && build/heap
	$(COMP) tests/avl.c -o build/avl -lshnet && build/avl
	$(COMP) tests/threads.c -o build/threads -lshnet && build/threads
	$(COMP) tests/time.c -o build/time -lshnet -lm && build/time
	$(COMP) tests/net.c -o build/net -lshnet && build/net
	$(COMP) tests/udp.c -o build/udp -lshnet && build/udp
	$(COMP) tests/tcp.c -o build/tcp -lshnet
	build/tcp 1 1 1
	build/tcp 1 1 0
	
	build/tcp 100 1 1
	build/tcp 100 1 0
	build/tcp 100 10 1
	build/tcp 100 10 0
	build/tcp 100 100 1
	build/tcp 100 100 0
	build/tcp 100 1000 1
	build/tcp 100 1000 0
	$(COMP) tests/tcp_stress.c -o build/tcp_stress -lshnet
	build/tcp_stress 1 1 1
	build/tcp_stress 1 1 0
	build/tcp_stress 1 2 1
	build/tcp_stress 1 2 0
	build/tcp_stress 1 3 1
	build/tcp_stress 1 3 0
	build/tcp_stress 1 4 1
	build/tcp_stress 1 4 0
	build/tcp_stress 1 5 1
	build/tcp_stress 1 5 0
	build/tcp_stress 1 6 1
	build/tcp_stress 1 6 0
	build/tcp_stress 1 7 1
	build/tcp_stress 1 7 0
	build/tcp_stress 1 8 1
	build/tcp_stress 1 8 0
	build/tcp_stress 1 9 1
	build/tcp_stress 1 9 0
	build/tcp_stress 1 10 1
	build/tcp_stress 1 10 0
	build/tcp_stress 1 15 1
	build/tcp_stress 1 15 0
	build/tcp_stress 1 20 1
	build/tcp_stress 1 20 0
	build/tcp_stress 1 25 1
	build/tcp_stress 1 25 0
	build/tcp_stress 1 30 1
	build/tcp_stress 1 30 0
	build/tcp_stress 1 35 1
	build/tcp_stress 1 35 0
	build/tcp_stress 1 40 1
	build/tcp_stress 1 40 0
	build/tcp_stress 1 45 1
	build/tcp_stress 1 45 0
	build/tcp_stress 1 50 1
	build/tcp_stress 1 50 0
	build/tcp_stress 1 60 1
	build/tcp_stress 1 60 0
	build/tcp_stress 1 70 1
	build/tcp_stress 1 70 0
	build/tcp_stress 1 80 1
	build/tcp_stress 1 80 0
	build/tcp_stress 1 90 1
	build/tcp_stress 1 90 0
	build/tcp_stress 1 100 1
	build/tcp_stress 1 100 0
	build/tcp_stress 1 200 1
	build/tcp_stress 1 200 0
	build/tcp_stress 1 1000 1
	build/tcp_stress 1 1000 0
	
	build/tcp_stress 100 1 1
	build/tcp_stress 100 1 0
	build/tcp_stress 100 2 1
	build/tcp_stress 100 2 0
	build/tcp_stress 100 3 1
	build/tcp_stress 100 3 0
	build/tcp_stress 100 4 1
	build/tcp_stress 100 4 0
	build/tcp_stress 100 5 1
	build/tcp_stress 100 5 0
	build/tcp_stress 100 6 1
	build/tcp_stress 100 6 0
	build/tcp_stress 100 7 1
	build/tcp_stress 100 7 0
	build/tcp_stress 100 8 1
	build/tcp_stress 100 8 0
	build/tcp_stress 100 9 1
	build/tcp_stress 100 9 0
	build/tcp_stress 100 10 1
	build/tcp_stress 100 10 0
	build/tcp_stress 100 15 1
	build/tcp_stress 100 15 0
	build/tcp_stress 100 20 1
	build/tcp_stress 100 20 0
	build/tcp_stress 100 25 1
	build/tcp_stress 100 25 0
	build/tcp_stress 100 30 1
	build/tcp_stress 100 30 0
	build/tcp_stress 100 35 1
	build/tcp_stress 100 35 0
	build/tcp_stress 100 40 1
	build/tcp_stress 100 40 0
	build/tcp_stress 100 45 1
	build/tcp_stress 100 45 0
	build/tcp_stress 100 50 1
	build/tcp_stress 100 50 0
	build/tcp_stress 100 60 1
	build/tcp_stress 100 60 0
	build/tcp_stress 100 70 1
	build/tcp_stress 100 70 0
	build/tcp_stress 100 80 1
	build/tcp_stress 100 80 0
	build/tcp_stress 100 90 1
	build/tcp_stress 100 90 0
	build/tcp_stress 100 100 1
	build/tcp_stress 100 100 0
	build/tcp_stress 100 200 1
	build/tcp_stress 100 200 0
	build/tcp_stress 100 1000 1
	build/tcp_stress 100 1000 0

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
	$(COMP) $(FILENAMES) -shared -o build/libshnet.so -lssl -lcrypto

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

build/misc.o: src/misc.h src/misc.c
	$(COMP) -fPIC -c src/misc.c -o build/misc.o

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

build/tcp.o: src/tcp.h src/tcp.c build/misc.o build/net.o
	$(COMP) -fPIC -c src/tcp.c -o build/tcp.o
