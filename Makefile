.PHONY: empty build test build-tests clean prepare include copy-headers build-static static strip-static build-dynamic dynamic strip-dynamic uninstall
empty: ;

CC := gcc
CFLAGS += -pthread -Wall -pedantic -O0
LDLIBS += -lshnet
LIBS += -lssl -lcrypto -lz -lbrotlienc -lbrotlidec
TESTLIBS += $(LIBS) -lm
DIR_IN  := src
DIR_OUT := bin
DIR_TEST := tests
DIR_INCLUDE := /usr/local/include/shnet
DIR_LIB := /usr/local/lib

ifneq ($(debug),)
CFLAGS += -D SHNET_DEBUG
endif

SOURCES = $(wildcard $(DIR_IN)/*.c)
OBJECTS = $(SOURCES:$(DIR_IN)/%.c=$(DIR_OUT)/%.o)

TEST_SUITES = $(wildcard $(DIR_TEST)/*.c)
TEST_EXECS = $(TEST_SUITES:$(DIR_TEST)/%.c=$(DIR_OUT)/test_%)

${DIR_OUT}:
	mkdir -p $@

shnet:
	mkdir -p shnet

${DIR_INCLUDE}:
	mkdir -p $(DIR_INCLUDE)

build: $(OBJECTS)

test: $(TEST_EXECS) | $(DIR_TEST)
	$(DIR_OUT)/test_heap
	$(DIR_OUT)/test_avl
	$(DIR_OUT)/test_threads
	$(DIR_OUT)/test_time
	
	$(DIR_OUT)/test_compression
	$(DIR_OUT)/test_http_p
	
	$(DIR_OUT)/test_tcp 1
	$(DIR_OUT)/test_tcp 4
	
	$(DIR_OUT)/test_tcp_stress 1
	$(DIR_OUT)/test_tcp_stress 4
	
	$(DIR_OUT)/test_tls 1
	$(DIR_OUT)/test_tls 4
	
	$(DIR_OUT)/test_tls_stress 1
	$(DIR_OUT)/test_tls_stress 4

${DIR_OUT}/test_%: $(DIR_TEST)/%.c $(DIR_TEST)/tests.h $(DIR_IN)/debug.c $(DIR_IN)/debug.h | $(DIR_OUT)
	$(CC) $(CFLAGS) $< -o $@ $(TESTLIBS) $(LDLIBS)

clean: | $(DIR_OUT)
	rm -r -f $(DIR_OUT) logs.txt
	mkdir -p $(DIR_OUT)

include: | $(DIR_INCLUDE)
	cp $(DIR_IN)/*.h $(DIR_INCLUDE)

copy-headers:
	cp $(DIR_IN)/*.h shnet

build-static: build
	ar rcsv $(DIR_OUT)/libshnet.a $(OBJECTS)
	ldconfig

static: build-static include
	cp $(DIR_OUT)/libshnet.a $(DIR_LIB)

strip-static: build-static copy-headers
	cp $(DIR_OUT)/libshnet.a shnet

build-dynamic: build
	$(CC) $(CFLAGS) $(OBJECTS) -shared -o $(DIR_OUT)/libshnet.so $(LIBS)

dynamic: build-dynamic include
	cp $(DIR_OUT)/libshnet.so $(DIR_LIB)
	ldconfig

strip-dynamic: build-dynamic copy-headers
	cp $(DIR_OUT)/libshnet.so shnet

uninstall:
	rm -f $(DIR_LIB)/libshnet.*
	rm -r -f $(DIR_INCLUDE) shnet


${DIR_OUT}/refheap.o: $(DIR_OUT)/heap.o

${DIR_OUT}/time.o: $(DIR_OUT)/threads.o

${DIR_OUT}/net.o: $(DIR_OUT)/threads.o

${DIR_OUT}/udp.o: $(DIR_OUT)/net.o

${DIR_OUT}/udplite.o: $(DIR_OUT)/udp.o

${DIR_OUT}/tcp.o: $(DIR_OUT)/net.o

${DIR_OUT}/tls.o: $(DIR_OUT)/tcp.o

${DIR_OUT}/http.o: $(DIR_OUT)/tls.o $(DIR_OUT)/compress.o $(DIR_OUT)/hash_table.o $(DIR_OUT)/http_p.o $(DIR_OUT)/time.o $(DIR_OUT)/base64.o

${DIR_OUT}/%.o: $(DIR_IN)/%.c $(DIR_IN)/%.h | $(DIR_OUT)
	$(CC) $(CFLAGS) -fPIC -c $< -o $@