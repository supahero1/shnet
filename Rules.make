SHELL := bash

BUILD_LANG ?= c
ifeq ($(BUILD_LANG),c)
BUILD_COMP := $(CC)
else
BUILD_COMP := $(CXX)
endif

BUILD_D := $(shell pwd)
REL_TOP := $(shell realpath $(DIR_TOP) --relative-to $(BUILD_D))
REL_OUT := $(REL_TOP)/$(DIR_OUT)

BUILD_SDIR := $(subst $(DIR_TOP)/,,$(BUILD_D))
BUILD_DEPS := $(REL_OUT)/deps/$(BUILD_SDIR)
BUILD_OUT  := $(REL_OUT)/$(BUILD_SDIR)

BUILD_INCL := $(BUILD_INCL:%=-I%)
B_LIBS     := $(shell ( \
for lib in $(BUILD_LIBS); do \
	if [[ $$lib == shnet* ]]; then \
		FPATH=$$(dirname $$(find $(BUILD_OUT) -name "lib$$lib.*")); \
  	echo "-L$$FPATH -Wl,-rpath,-L$$FPATH"; \
	fi \
done \
) | tr '\n' ' ' )
BUILD_LIBS := $(BUILD_LIBS:%=-l%)

ifneq ($(origin BUILD_SRC),file)
BUILD_SRC  := $(wildcard *.$(BUILD_LANG))
endif
DEPS       := $(BUILD_SRC:%=$(BUILD_DEPS)/%.d)

.PHONY: only
only:
	@printf ">>> START <<<\n\n$(BUILD_LIBS)\n\n$(B_LIBS)\n\n>>>  END  <<<\n"

ifeq ($(BUILD_EXE),1)
BUILD_SRC  := $(BUILD_SRC:%.$(BUILD_LANG)=%)
else
BUILD_SRC  := $(BUILD_SRC:.$(BUILD_LANG)=.o)
endif
ifdef BUILD_SO
BUILD_SRC  += lib$(BUILD_SO).so
endif

ifeq ($(BUILD_USE),1)
ifeq ($(VALGRIND),1)
BUILD_FLAGS  += -DSHNET_TEST_VALGRIND
endif
endif

_BUILD_FILES := $(BUILD_SRC:%=$(BUILD_OUT)/%)

.PRECIOUS: $(_BUILD_FILES)

.PHONY: build_obj
build_obj: subdirs $(DEPS) $(_BUILD_FILES)

${BUILD_OUT}/%.o: %.$(BUILD_LANG)
	$(Q)$(BUILD_COMP) $(CFLAGS) $(BUILD_FLAGS) \
			-fPIC -c -o $@ $< -I$(DIR_HEADERS)

${BUILD_OUT}/%: %.$(BUILD_LANG)
	$(Q)$(BUILD_COMP) $(CFLAGS) $(BUILD_FLAGS) -o $@ $< \
			-I$(DIR_HEADERS) $(BUILD_INCL) $(_LIBS)$(CLIBS) \
			$(BUILD_LIBS)
ifeq ($(BUILD_USE),1)
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
			$(BUILD_INCL) $(_LIBS)$(CLIBS) $(BUILD_LIBS)
	$(Q)ln -f $@ $(basename $@)

${BUILD_DEPS}/%.d: % | $(BUILD_DEPS)
	$(Q)$(BUILD_COMP) -MM -I$(DIR_HEADERS) -MF $@ \
	 		-MT $(BUILD_OUT)/$(basename $(notdir $<)).o $<

.PHONY: subdirs
subdirs:
ifdef BUILD_SUBDIRS
	$(Q)set -e; for i in $(BUILD_SUBDIRS); do $(MAKE) -C $$i; done
endif

#include $(DEPS)
