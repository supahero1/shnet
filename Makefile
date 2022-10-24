.EXPORT_ALL_VARIABLES:

include Consts.make

ifeq ($(VERBOSE),1)
Q = 
else
Q = @
COVFLAGS = -q
endif

CFLAGS := -Wall -Wextra -flto -O3
ifeq ($(DEBUG),1)
CFLAGS += -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=3 -g3
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

.PHONY: build
build:
	$(Q)./sed.sh
	$(Q)mkdir -p $(DIR_OUT)
	$(Q)$(MAKE) -C src
	$(Q)$(MAKE) -C src -f Library.make
	$(Q)$(MAKE) -C cli
	$(Q)$(MAKE) -C cli -f Executable.make
	$(FMT_OD)Building complete.$(FMT_DO)

.PHONY: install
install: build
	$(Q)mkdir -p $(DIR_INCLUDE) $(DIR_LIB) $(DIR_BIN)
ifeq ($(STATIC),1)
	$(Q)$(RM) $(DIR_LIB)/libshnet.so
	$(Q)install $(DIR_OUT)/src/libshnet.a $(DIR_LIB)/
else
	$(Q)$(RM) $(DIR_LIB)/libshnet.a
	$(Q)install $(DIR_OUT)/src/libshnet.so $(DIR_LIB)/
endif
	$(Q)ldconfig $(DIR_LIB)
	$(Q)cp -r include/shnet $(DIR_INCLUDE)/
	$(Q)install $(DIR_OUT)/cli/shnet $(DIR_BIN)/
	$(FMT_OD)Installation complete.$(FMT_DO)

.PHONY: test
test: build
ifeq ($(COVERAGE),1)
	$(Q)mkdir -p coverage
endif
	$(Q)$(RM) $(shell find $(DIR_OUT)/src -name *.gcda)
ifneq ($(PRESERVE_TESTS),1)
	$(Q)$(RM) -r $(DIR_OUT)/tests
endif
	$(Q)$(MAKE) -C tests
ifeq ($(COVERAGE),1)
	$(Q)lcov $(COVFLAGS) -c -o coverage/coverage.info \
			-d $(DIR_OUT)/src
	$(Q)genhtml coverage/coverage.info \
			$(COVFLAGS) -o coverage
	$(FMT_OD)file://$(DIR_TOP)/coverage/index.html$(FMT_DO)
endif
	$(FMT_OD)Testing complete.$(FMT_DO)

.PHONY: clean
clean:
	$(Q)$(RM) -r $(DIR_OUT) coverage
	$(Q)./unsed.sh
	$(FMT_OD)Clean complete.$(FMT_DO)

.PHONY: uninstall
uninstall:
	$(Q)$(RM) -r $(DIR_INCLUDE)/shnet $(DIR_BIN)/shnet \
			$(DIR_LIB)/libshnet.so $(DIR_LIB)/libshnet.a
	$(FMT_OD)Uninstall complete.$(FMT_DO)

.PHONY: help
help:
	$(Q)cat INSTALL

include Rules.make
