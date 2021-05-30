empty: ;
.PHONY: empty build test clean prepare include copy-headers build-static static strip-static build-dynamic dynamic strip-dynamic uninstall

CMPE_CORE=gcc -pthread -Wall -pedantic -D_GNU_SOURCE
CMPE=O3
ifneq ($(debug),)
COMP=$(CMPE_CORE) -$(CMPE) -D SHNET_DEBUG
else
COMP=$(CMPE_CORE) -$(CMPE)
endif
FILENAMES=build/debug.o build/heap.o build/refheap.o build/avl.o build/threads.o build/misc.o build/time.o build/net.o #build/udp.o
SOURCES=src/debug.c src/debug.h src/heap.c src/heap.h src/refheap.c src/refheap.h src/avl.c src/avl.h src/threads.c src/threads.h src/misc.c src/misc.h src/time.c src/time.h src/net.c src/net.h #src/udp.c src/udp.h

build: prepare $(FILENAMES) $(SOURCES)

test: $(wildcard tests/*.c) $(wildcard tests/*.h)
	$(COMP) tests/heap.c -o build/heap -lshnet && build/heap
	$(COMP) tests/avl.c -o build/avl -lshnet && build/avl
	$(COMP) tests/threads.c -o build/threads -lshnet && build/threads
	$(COMP) tests/time.c -o build/time -lshnet -lm && build/time
	$(COMP) tests/net.c -o build/net -lshnet && build/net
	#$(COMP) tests/udp.c -o build/udp -lshnet && build/udp

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
	$(COMP) $(FILENAMES) -shared -o build/libshnet.so

dynamic: build-dynamic include
	cp build/libshnet.so /usr/lib/
	cp build/libshnet.so /usr/local/lib/

strip-dynamic: build-dynamic copy-headers
	cp build/libshnet.so shnet/

uninstall:
	rm -f /usr/lib/libshnet.* /usr/local/lib/libshnet.*
	rm -r -f /usr/include/shnet /usr/local/include/shnet shnet


build/debug.o: $(wildcard src/debug.*)
	$(COMP) -fPIC -c src/debug.c -o build/debug.o

build/heap.o: $(wildcard src/heap.*)
	$(COMP) -fPIC -c src/heap.c -o build/heap.o

build/refheap.o: $(wildcard src/refheap.*) $(wildcard src/heap.*)
	$(COMP) -fPIC -c src/refheap.c -o build/refheap.o

build/avl.o: $(wildcard src/avl.*)
	$(COMP) -fPIC -c src/avl.c -o build/avl.o

build/threads.o: $(wildcard src/threads.*)
	$(COMP) -fPIC -c src/threads.c -o build/threads.o

build/misc.o: $(wildcard src/misc.*)
	$(COMP) -fPIC -c src/misc.c -o build/misc.o

build/time.o: $(wildcard src/time.*) $(wildcard src/refheap.*) $(wildcard src/threads.*)
	$(COMP) -fPIC -c src/time.c -o build/time.o

build/net.o: $(wildcard src/net.*) $(wildcard src/misc.*) $(wildcard src/threads.*)
	$(COMP) -fPIC -c src/net.c -o build/net.o

#build/udp.o: $(wildcard src/udp.*) $(wildcard src/net.*)
	#$(COMP) -fPIC -c src/udp.c -o build/udp.o
