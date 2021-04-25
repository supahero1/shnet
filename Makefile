empty: ;
.PHONY: empty build clean prepare test include copy-headers build-static static strip-static build-dynamic dynamic strip-dynamic uninstall

CMPE_CORE=gcc -pthread -Wall -pedantic -D_GNU_SOURCE
CMPE=O3 -fomit-frame-pointer
ifneq ($(debug),)
COMP=$(CMPE_CORE) -$(CMPE) -D NET_DEBUG
else
COMP=$(CMPE_CORE) -$(CMPE)
endif

build: prepare build/flesto.o

test: prepare tests/tests.h src/debug.h tests/flesto.c
	$(COMP) tests/flesto.c src/flesto.c -o build/flesto && build/flesto

clean:
	rm -r -f build

prepare:
	mkdir -p build

build/flesto.o: src/flesto.c src/flesto.h
	$(COMP) -fPIC -c src/flesto.c -o build/flesto.o

include: build
	mkdir -p /usr/include/shnet
	cp src/flesto.h /usr/include/shnet/
	mkdir -p /usr/local/include/shnet
	cp src/flesto.h /usr/local/include/shnet/

copy-headers:
	mkdir -p shnet
	cp src/flesto.h shnet/

build-static: build
	ar rcsv build/libshnet.a $(wildcard build/*.o)

static: build-static include
	cp build/libshnet.a /usr/lib/
	cp build/libshnet.a /usr/local/lib/

strip-static: build-static copy-headers
	cp build/libshnet.a shnet/

build-dynamic: build
	$(COMP) $(wildcard build/*.o) -shared -o build/libshnet.so

dynamic: build-dynamic include
	cp build/libshnet.so /usr/lib/
	cp build/libshnet.so /usr/local/lib/

strip-dynamic: build-dynamic copy-headers
	cp build/libshnet.so shnet/

uninstall:
	rm -f /usr/lib/libshnet.* /usr/local/lib/libshnet.*
	rm -r -f /usr/include/shnet /usr/local/include/shnet shnet