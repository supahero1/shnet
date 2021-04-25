empty: ;
.PHONY: empty debug build clean prepare test include static dynamic strip-static strip-dynamic uninstall

CMPE=gcc -O3 -pthread -Wall -pedantic -D_GNU_SOURCE -fomit-frame-pointer

debug: CMPE += -D NET_DEBUG
debug: build
	

build: prepare build/flesto.o
	

test: tests/tests.h src/debug.h tests/flesto.c
	${CMPE} tests/flesto.c src/flesto.c -o tests/flesto && tests/flesto

clean:
	rm -r -f build tests/flesto tests/logs.txt

prepare:
	mkdir -p build

build/flesto.o: src/flesto.c src/flesto.h
	${CMPE} -fPIC -c src/flesto.c -o build/flesto.o

include: build
	mkdir -p /usr/include/shnet
	cp src/flesto.h /usr/include/shnet/
	mkdir -p /usr/local/include/shnet
	cp src/flesto.h /usr/local/include/shnet/

copy-headers:
	mkdir -p shnet
	cp src/flesto.h shnet/

static: include
	ar rcsv build/libshnet.a $(wildcard build/*.o)
	cp build/libshnet.a /usr/lib/
	cp build/libshnet.a /usr/local/lib/

strip-static: build copy-headers
	ar rcsv build/libshnet.a $(wildcard build/*.o)
	cp build/libshnet.a shnet/

dynamic: include
	${CMPE} $(wildcard build/*.o) -shared -o build/libshnet.so
	cp build/libshnet.so /usr/lib/
	cp build/libshnet.so /usr/local/lib/

strip-dynamic: build copy-headers
	${CMPE} $(wildcard build/*.o) -shared -o build/libshnet.so
	cp build/libshnet.so shnet/

uninstall:
	rm -f /usr/lib/libshnet.* /usr/local/lib/libshnet.*
	rm -r -f /usr/include/shnet /usr/local/include/shnet shnet