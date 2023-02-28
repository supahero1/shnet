.EXPORT_ALL_VARIABLES:

include Consts.make

.PHONY: build
build:
	$(Q)./sed.sh
	$(Q)$(MAKE) -C src
ifneq ($(NO_CLI),1)
	$(Q)$(MAKE) -C cli
endif
	$(FMT_OD)Building complete.$(FMT_DO)

.PHONY: install
install: build
	$(Q)mkdir -p $(DIR_INCLUDE) $(DIR_LIB) $(DIR_BIN)
ifeq ($(STATIC),1)
	$(Q)$(RM) $(DIR_LIB)/lib$(PROJECT_NAME).so
	$(Q)install $(DIR_OUT)/src/lib$(PROJECT_NAME).a $(DIR_LIB)/
else
	$(Q)$(RM) $(DIR_LIB)/lib$(PROJECT_NAME).a
	$(Q)install $(DIR_OUT)/src/lib$(PROJECT_NAME).so $(DIR_LIB)/
endif
	$(Q)ldconfig $(DIR_LIB)
	$(Q)cp -r include/$(PROJECT_NAME) $(DIR_INCLUDE)/
	$(Q)install $(DIR_OUT)/cli/$(PROJECT_NAME) $(DIR_BIN)/
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
	$(Q)lcov $(COVFLAGS) -c -o coverage/coverage.info -d $(DIR_OUT)/src
	$(Q)genhtml coverage/coverage.info $(COVFLAGS) -o coverage
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
	$(Q)$(RM) -r $(DIR_INCLUDE)/$(PROJECT_NAME) $(DIR_BIN)/$(PROJECT_NAME) \
		$(DIR_LIB)/lib$(PROJECT_NAME).so $(DIR_LIB)/lib$(PROJECT_NAME).a
	$(FMT_OD)Uninstall complete.$(FMT_DO)

.PHONY: help
help:
	$(Q)cat INSTALL

.PHONY: replace
replace:
	rm -fr /usr/local/include/shnet
	cp -r include/shnet /usr/local/include/
