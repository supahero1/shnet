VERSION = 1
PATCH = 3

CLI_VERSION = 1
CLI_PATCH = 0

.EXPORT_ALL_VARIABLES:

.PHONY: build
build: __build

ifeq ($(VERBOSE),1)
Q = 
else
Q = @
COVFLAGS = -q
endif

SHELL  := bash

FMT_OD := "$(shell tput bold)
FMT_DO :=  $(shell tput sgr0)\n"

CC     := gcc
CXX    := g++
CFLAGS := -Wall -Wextra -flto
ifeq ($(DEBUG),1)
CFLAGS += -D_FORTIFY_SOURCE=3 -Og -g3
else
CFLAGS += -O3
endif
CLIBS  :=
ifeq ($(WITH_LIBUV),1)
CLIBS  += -luv
CFLAGS += -DLIBUV
endif
ifeq ($(COVERAGE),1)
CFLAGS += --coverage
endif
CLIBS  += -pthread#-lssl -lcrypto

DIR_TOP     := $(shell pwd)
DIR_OUT     := $(DIR_TOP)/bin
DIR_TEST    := $(DIR_TOP)/tests
DIR_DEPS    := $(DIR_OUT)/deps
DIR_HEADERS := $(DIR_TOP)/include
DIR_INCLUDE := /usr/local/include
DIR_LIB     := /usr/local/lib
DIR_COV     := $(DIR_TOP)/coverage
DIR_BIN     := /usr/local/bin

${DIR_OUT} ${DIR_DEPS} ${DIR_INCLUDE}/shnet \
${DIR_LIB} ${DIR_COV} ${DIR_BIN}:
	$(Q)mkdir -p $@

.PHONY: _build
_build: | $(DIR_OUT) $(DIR_DEPS)
	$(Q)chmod +x $(DIR_TOP)/sed_in
	$(Q)$(DIR_TOP)/sed_in
	$(Q)$(MAKE) -C $(DIR_TOP)/src
	$(Q)$(MAKE) -C $(DIR_TOP)/cli

.PHONY: __build
ifeq ($(STATIC),1)
__build: _build $(DIR_OUT)/src/libshnet.a \
         $(DIR_OUT)/cli/shnet.exe
else
__build: _build $(DIR_OUT)/src/libshnet.so \
				 $(DIR_OUT)/cli/shnet.exe
endif
	@printf $(FMT_OD)Building complete.$(FMT_DO)

.PHONY: install
install: build | $(DIR_LIB) $(DIR_BIN) $(DIR_INCLUDE)/shnet
ifeq ($(STATIC),1)
	$(Q)$(RM) $(DIR_LIB)/libshnet.so
	$(Q)install $(DIR_OUT)/src/libshnet.a $(DIR_LIB)/
else
	$(Q)$(RM) $(DIR_LIB)/libshnet.a
	$(Q)install $(DIR_OUT)/src/libshnet.so $(DIR_LIB)/
endif
	$(Q)ldconfig $(DIR_LIB)
	$(Q)cp -r $(DIR_HEADERS)/shnet $(DIR_INCLUDE)/
	$(Q)install $(DIR_OUT)/cli/shnet $(DIR_BIN)/
	@printf $(FMT_OD)Installation complete.$(FMT_DO)

.PHONY: test
ifeq ($(COVERAGE),1)
test: build | $(DIR_COV)
	$(Q)$(RM) $(shell find $(DIR_OUT)/src -name *.gcda)
else
test: build
endif
ifneq ($(PRESERVE_TESTS),1)
	$(Q)$(RM) -r $(DIR_OUT)/tests
endif
	$(Q)$(MAKE) -C $(DIR_TEST)
ifeq ($(COVERAGE),1)
	$(Q)lcov $(COVFLAGS) -c -o $(DIR_COV)/coverage.info \
			-d $(DIR_OUT)/src
	$(Q)genhtml $(DIR_COV)/coverage.info \
			$(COVFLAGS) -o $(DIR_COV)
	@printf $(FMT_OD)Coverage in file://$(DIR_COV)/index.html$(FMT_DO)
endif
	@printf $(FMT_OD)Testing complete.$(FMT_DO)

.PHONY: clean
clean:
	$(Q)$(RM) -r $(DIR_DEPS) $(DIR_OUT) $(DIR_COV)
	$(Q)chmod +x $(DIR_TOP)/unsed_in
	$(Q)$(DIR_TOP)/unsed_in
	@printf $(FMT_OD)Clean complete.$(FMT_DO)

.PHONY: uninstall
uninstall:
	$(Q)$(RM) -r $(DIR_INCLUDE)/shnet $(DIR_BIN)/shnet \
			$(DIR_LIB)/libshnet.so $(DIR_LIB)/libshnet.a
	@printf $(FMT_OD)Uninstall complete.$(FMT_DO)

.PHONY: help
help:
	$(Q)cat INSTALL

include Rules.make
