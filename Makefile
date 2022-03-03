.PHONY: empty build test build-tests clean uninstall static dynamic
empty: ;

BASE_FLAGS := -Wall -Wextra -Wno-newline-eof -Wno-unused-parameter -pedantic
CFLAGS     += $(BASE_FLAGS) -O3
CXXFLAGS   += $(BASE_FLAGS) -Wno-c11-extensions -Wno-gnu-anonymous-struct -Wno-nested-anon-types

DIR_IN      := src
DIR_OUT     := bin
DIR_TEST    := tests
DIR_HEADERS := include
DIR_INCLUDE := /usr/local/include
DIR_LIB     := /usr/local/lib

SOURCES = $(wildcard $(DIR_IN)/*.c)
OBJECTS = $(SOURCES:$(DIR_IN)/%.c=$(DIR_OUT)/%.o)

TEST_SUITES = $(wildcard $(DIR_TEST)/*.c)
TEST_EXECS  = $(TEST_SUITES:$(DIR_TEST)/%.c=$(DIR_OUT)/test_%)

${DIR_OUT}:
	mkdir -p $@

build: $(OBJECTS)

build-tests: build $(TEST_EXECS) | $(DIR_OUT)

test: build-tests $(DIR_OUT)/cc_compat
	$(DIR_OUT)/test_threads
	$(DIR_OUT)/test_thread_pool
	$(DIR_OUT)/test_time
	$(DIR_OUT)/test_async
	$(DIR_OUT)/test_tcp
	$(DIR_OUT)/test_tcp_bench -b
	$(DIR_OUT)/test_tcp_bench -b -C 8 -S 8 -s
	$(DIR_OUT)/test_tcp_bench -c
	$(DIR_OUT)/test_tcp_bench -c -C 8 -S 8 -s

clean:
	rm -r -f $(DIR_OUT) logs.txt

uninstall:
	rm -f $(DIR_LIB)/libshnet.a $(DIR_LIB)/libshnet.so
	rm -r -f $(DIR_INCLUDE)/shnet

static: build
	ar rcsv $(DIR_OUT)/libshnet.a $(OBJECTS)
	cp $(DIR_OUT)/libshnet.a $(DIR_LIB)
	cp -r $(DIR_HEADERS)/* $(DIR_INCLUDE)/
	ldconfig

dynamic: build
	$(CC) $(OBJECTS) -shared -o $(DIR_OUT)/libshnet.so
	cp $(DIR_OUT)/libshnet.so $(DIR_LIB)
	cp -r $(DIR_HEADERS)/* $(DIR_INCLUDE)/
	ldconfig


${DIR_OUT}/cc_compat: $(DIR_TEST)/cc_compat.cc | $(DIR_OUT)
	clang++ $(CXXFLAGS) $(DIR_TEST)/cc_compat.cc -o $(DIR_OUT)/cc_compat -Iinclude

${DIR_OUT}/test_%: $(DIR_TEST)/%.c $(DIR_HEADERS)/shnet/tests.h | build $(DIR_OUT)
	$(CC) $(CFLAGS) $< $(OBJECTS) -o $@ -Iinclude -lm

${DIR_OUT}/test_threads: $(DIR_OUT)/threads.o

${DIR_OUT}/test_thread_pool: $(DIR_OUT)/threads.o

${DIR_OUT}/test_time: $(DIR_OUT)/time.o

${DIR_OUT}/test_bench_time: $(DIR_OUT)/time.o

${DIR_OUT}/test_async: $(DIR_OUT)/async.o

${DIR_OUT}/test_tcp: $(DIR_OUT)/tcp.o

${DIR_OUT}/test_tcp_bench: $(DIR_OUT)/tcp.o


${DIR_OUT}/%.o: $(DIR_IN)/%.c $(DIR_HEADERS)/shnet/%.h $(DIR_HEADERS)/shnet/error.h | $(DIR_OUT)
	$(CC) $(CFLAGS) -fPIC -c $< -o $@ -Iinclude

${DIR_OUT}/async.o: $(DIR_OUT)/threads.o

${DIR_OUT}/time.o: $(DIR_OUT)/threads.o

${DIR_OUT}/tcp.o: $(DIR_OUT)/storage.o $(DIR_OUT)/threads.o $(DIR_OUT)/async.o $(DIR_OUT)/net.o