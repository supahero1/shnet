BUILD_LANG ?= c
ifeq ($(BUILD_LANG),c)
BUILD_COMP := $(CC)
else
BUILD_COMP := $(CXX)
endif

BUILD_SDIR := $(subst $(DIR_TOP)/,,$(shell pwd))
BUILD_DEPS := $(DIR_DEPS)/$(BUILD_SDIR)
BUILD_OUT  := $(DIR_OUT)/$(BUILD_SDIR)

ifneq ($(origin BUILD_SRC),file)
BUILD_SRC  := $(wildcard *.$(BUILD_LANG))
endif
SAVED_SRC  := $(BUILD_SRC)
DEPS       := $(BUILD_SRC:%=$(BUILD_DEPS)/%.d)
ifdef BUILD_EXE
BUILD_SRC  := $(BUILD_SRC:%.$(BUILD_LANG)=%)
else
BUILD_SRC  := $(BUILD_SRC:.$(BUILD_LANG)=.o)
endif

ifdef BUILD_USE
ifeq ($(VALGRIND),1)
BUILD_FLAGS  += -DSHNET_TEST_VALGRIND
endif
endif

_BUILD_FILES := $(BUILD_SRC:%=$(BUILD_OUT)/%)

.PRECIOUS: $(_BUILD_FILES)

.PHONY: build_obj
build_obj: subdirs $(DEPS) $(_BUILD_FILES)

${BUILD_DEPS} ${BUILD_OUT}:
	$(Q)mkdir -p $@

${BUILD_OUT}/%.o: %.$(BUILD_LANG) | $(BUILD_OUT)
	$(Q)$(BUILD_COMP) $(CFLAGS) $(BUILD_FLAGS) \
			-fPIC -c -o $@ $< -I$(DIR_HEADERS)

${BUILD_OUT}/%: %.$(BUILD_LANG) | $(BUILD_OUT)
	$(Q)$(BUILD_COMP) $(CFLAGS) $(BUILD_FLAGS) -o $@ $< \
			-I$(DIR_HEADERS) -L$(DIR_OUT)/src \
			-Wl,-rpath,$(DIR_OUT)/src $(CLIBS) \
			$(BUILD_LIBS) -lshnet
ifdef BUILD_USE
ifeq ($(VALGRIND),1)
	$(Q) valgrind --track-origins=yes --leak-check=full \
		--show-leak-kinds=all $@ #--gen-suppressions=all
else
	$(Q)$@
endif
endif

.SECONDEXPANSION:

${DIR_OUT}/%.so: $$(shell find $$(dir $$@) -name *.o)
	$(Q)$(CC) $(CFLAGS) -o $@ $(CLIBS) -shared $^

${DIR_OUT}/%.a: $$(shell find $$(dir $$@) -name *.o)
	$(Q)$(AR) rsc $@ $^

${DIR_OUT}/%.exe: $$(shell find $$(dir $$@) -name *.o)
	$(Q)$(CC) $(CFLAGS) -o $@ $^ -I$(DIR_HEADERS) \
			-L$(DIR_OUT)/src -Wl,-rpath,$(DIR_OUT)/src \
			$(CLIBS) -lshnet
	$(Q)ln -f $@ $(basename $@)

${BUILD_DEPS}/%.d: % | $(BUILD_DEPS)
	$(Q)$(BUILD_COMP) -MM -I$(DIR_HEADERS) -MF $@ \
	 		-MT $(BUILD_OUT)/$(basename $(notdir $<)).o $<

.PHONY: subdirs
subdirs:
ifdef BUILD_SUBDIRS
	$(Q)set -e; for i in $(BUILD_SUBDIRS); do $(MAKE) -C $$i; done
endif

include $(DEPS)
