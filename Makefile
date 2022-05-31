VERSION = 1
PATCH = 3

CLI_VERSION = 1
CLI_PATCH = 0

.EXPORT_ALL_VARIABLES:

ifeq ($(VERBOSE),1)
Q = 
else
Q = @
COVFLAGS = -q
endif

CC     := gcc
CXX    := g++
CFLAGS := -Wall -Wextra -flto
ifeq ($(DEBUG),1)
CFLAGS += -Og -g3
else
CFLAGS += -O3
endif
ifeq ($(COVERAGE),1)
CFLAGS += --coverage
endif
CLIBS  := -pthread #-lssl -lcrypto

DIR_TOP      := $(shell pwd)
DIR_IN       := $(DIR_TOP)/src
DIR_OUT      := $(DIR_TOP)/bin
DIR_LIB_OUT  := $(DIR_OUT)/lib
DIR_TEST     := $(DIR_TOP)/tests
DIR_TEST_OUT := $(DIR_OUT)/tests
DIR_DEPS     := $(DIR_OUT)/deps
DIR_HEADERS  := $(DIR_TOP)/include
DIR_INCLUDE  := /usr/local/include
DIR_LIB      := /usr/local/lib
DIR_COVERAGE := $(DIR_TOP)/coverage
DIR_CLI      := $(DIR_TOP)/cli
DIR_CLI_OUT  := $(DIR_OUT)/cli
DIR_BIN      := /usr/bin

.PHONY: build
build: | $(DIR_CLI_OUT)
	$(Q)chmod u+x $(DIR_TOP)/sed_in
	$(Q)chmod u+x $(DIR_TOP)/unsed_in
	$(Q)$(DIR_TOP)/sed_in
	$(Q)$(MAKE) -C $(DIR_IN)
ifeq ($(STATIC),1)
	$(Q)$(RM) $(DIR_LIB)/libshnet.so \
			$(DIR_LIB_OUT)/libshnet.so
	$(Q)$(AR) rsc $(DIR_LIB_OUT)/libshnet.a \
			$(DIR_LIB_OUT)/*.o
else
	$(Q)$(RM) $(DIR_LIB)/libshnet.a \
			$(DIR_LIB_OUT)/libshnet.a
	$(Q)$(CC) $(CFLAGS) $(DIR_LIB_OUT)/*.o -shared \
			-o $(DIR_LIB_OUT)/libshnet.so $(CLIBS)
endif
	$(Q)ldconfig $(DIR_LIB_OUT)
	$(Q)#$(MAKE) -C $(DIR_CLI)
	$(Q)#$(CC) $(CFLAGS) $(DIR_CLI_OUT)/*.o -lshnet \
	#		-o $(DIR_CLI_OUT)/shnet -I$(DIR_HEADERS) \
	#		-L$(DIR_LIB_OUT) $(CLIBS)
	@echo "Building complete."

.PHONY: install
install: build | $(DIR_LIB) $(DIR_BIN) $(DIR_INCLUDE)/shnet
ifeq ($(STATIC),1)
	$(Q)install $(DIR_LIB_OUT)/libshnet.a $(DIR_LIB)/
else
	$(Q)install $(DIR_LIB_OUT)/libshnet.so $(DIR_LIB)/
endif
	$(Q)cp -r $(DIR_HEADERS)/shnet $(DIR_INCLUDE)/
	$(Q)ldconfig $(DIR_LIB_OUT)
	$(Q)#install $(DIR_CLI_OUT)/shnet $(DIR_BIN)/
	@echo "Installation complete."

.PHONY: test
ifeq ($(COVERAGE),1)
test: build | $(DIR_COVERAGE)
	$(Q)$(RM) $(DIR_LIB_OUT)/*.gcda
else
test: build
endif
	$(Q)$(RM) -r $(DIR_TEST_OUT)
	$(Q)$(MAKE) -C $(DIR_TEST)
	@echo "Testing complete."
ifeq ($(COVERAGE),1)
	$(Q)lcov $(COVFLAGS) -c -d $(DIR_LIB_OUT) -o \
			$(DIR_LIB_OUT)/coverage.info
	$(Q)genhtml $(DIR_LIB_OUT)/coverage.info \
			$(COVFLAGS) -o $(DIR_COVERAGE)
	@echo "Coverage in file:$(DIR_COVERAGE)/index.html"
endif
	@echo "Testing complete."

.PHONY: clean
clean:
	$(Q)$(RM) -r $(DIR_OUT) $(DIR_COVERAGE)
	$(Q)$(DIR_TOP)/unsed_in
	@echo "Clean complete."

.PHONY: uninstall
uninstall:
	$(Q)$(RM) -r $(DIR_INCLUDE)/shnet $(DIR_BIN)/shnet \
			$(DIR_LIB)/libshnet.so $(DIR_LIB)/libshnet.a
	@echo "Uninstall complete."

.PHONY: help
help:
	$(Q)cat INSTALL

include Rules.make
