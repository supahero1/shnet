BUILD_LANG ?= c
ifeq ($(BUILD_LANG),c)
BUILD_COMP := $(CC)
else
BUILD_COMP := $(CXX)
endif
DEPS       := $(BUILD_SRC:%=$(DIR_DEPS)/%.d)
ifdef BUILD_EXE
BUILD_SRC  := $(BUILD_SRC:%.$(BUILD_LANG)=%)
else
BUILD_SRC  := $(BUILD_SRC:.$(BUILD_LANG)=.o)
endif

ifdef BUILD_USE
ifeq ($(VALGRIND),1)
BUILD_FLAGS += -DSHNET_TEST_VALGRIND
endif
endif

_BUILD_FILES := $(BUILD_SRC:%=$(BUILD_OUT)/%)

.PRECIOUS: $(_BUILD_FILES)

.PHONY: build_obj
build_obj: subdirs $(DEPS) $(_BUILD_FILES)

${DIR_OUT} ${DIR_LIB_OUT} ${DIR_TEST_OUT} ${DIR_COVERAGE} \
${DIR_INCLUDE}/shnet ${DIR_LIB} ${DIR_DEPS} ${DIR_CLI_OUT}:
	$(Q)mkdir -p $@

${BUILD_OUT}/%.o: %.$(BUILD_LANG) | $(BUILD_OUT)
	$(Q)$(BUILD_COMP) $(CFLAGS) $(BUILD_FLAGS) -fPIC -c \
			-o $@ $< -I$(DIR_HEADERS) $(CLIBS) $(BUILD_LIBS)

${BUILD_OUT}/%: %.$(BUILD_LANG) | $(BUILD_OUT)
	$(Q)$(BUILD_COMP) $(CFLAGS) $(BUILD_FLAGS) -o $@ $< \
			-I$(DIR_HEADERS) -L$(DIR_LIB_OUT) $(CLIBS) $(BUILD_LIBS) \
			-lshnet
ifdef BUILD_USE
ifeq ($(VALGRIND),1)
	$(Q) valgrind --track-origins=yes --leak-check=full \
		--show-leak-kinds=all $@ #--gen-suppressions=all
else
	$(Q)$@
endif
endif

${DIR_DEPS}/%.d: % | $(DIR_DEPS)
	$(Q)$(BUILD_COMP) -MM -I$(DIR_HEADERS) -MF $@ \
	 		-MT $(BUILD_OUT)/$(basename $(notdir $@)).o $<

.PHONY: subdirs
subdirs:
ifdef BUILD_SUBDIRS
	set -e; for i in $(BUILD_SUBDIRS); do $(MAKE) -C $$i; done
endif

include $(DEPS)
