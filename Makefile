empty: ;
.PHONY: empty build test clean prepare include copy-headers build-static static strip-static build-dynamic dynamic strip-dynamic uninstall

CMPE_CORE=gcc -pthread -Wall -pedantic -D_GNU_SOURCE
CMPE=O3 -fomit-frame-pointer
ifneq ($(debug),)
COMP=$(CMPE_CORE) -$(CMPE) -D SHNET_DEBUG
else
COMP=$(CMPE_CORE) -$(CMPE)
endif
FILENAMES=build/debug.o

build: prepare $(FILENAMES)

test: $(wildcard tests/*.c) $(wildcard tests/*.h)
	#$(COMP) tests/aaa.c -o build/aaa -lshnet && build/aaa

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


build/debug.o:
	$(COMP) -fPIC -c src/debug.c -o build/debug.o

build/heap.o:
	$(COMP) -fPIC -c src/heap.c -o build/heap.o
