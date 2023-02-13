SHELL := bash

BUILD_LANG ?= c
ifeq ($(BUILD_LANG),c)
BUILD_COMP := $(CC)
BUILD_FLAGS += $(CFLAGS)
else
BUILD_COMP := $(CXX)
BUILD_FLAGS += $(CXXFLAGS)
endif

BUILD_TYPE ?= $(BUILD_O)

BUILD_D := $(shell pwd)
REL_TOP := $(shell realpath $(DIR_TOP) --relative-to $(BUILD_D))
REL_OUT := $(REL_TOP)/$(DIR_OUT)

BUILD_SDIR := $(subst $(DIR_TOP)/,,$(BUILD_D))
BUILD_DEPS := $(REL_OUT)/deps/$(BUILD_SDIR)
BUILD_OUT  := $(REL_OUT)/$(BUILD_SDIR)

ifneq ($(BUILD_INCL),)
BUILD_INCL := $(shell echo $$(realpath $(BUILD_INCL) --relative-to $(BUILD_D)))
BUILD_INCL := $(BUILD_INCL:%=-I%)
endif
LIBS_UNPR  := $(shell echo $$( \
for lib in $(BUILD_LIBS); do \
	if [[ $$lib == $(PROJECT_NAME)* ]]; then \
		FILE=$$(find $(REL_OUT) -name "lib$$lib.*"); \
		if [ ! -z "$$FILE" ]; then \
			echo "$$(dirname $$FILE)"; \
		fi \
	fi \
done \
))
empty :=
space := $(empty) $(empty)
LIBS_PATH  := $(subst $(space),:,$(LIBS_UNPR))
B_LIBS     := $(LIBS_UNPR:%=-L%)
BUILD_LIBS := $(BUILD_LIBS:%=-l%)

BUILD_INCLUDES  := -I$(REL_TOP)/include/ $(BUILD_INCL)
BUILD_LIBRARIES := $(BUILD_LIBS) $(B_LIBS) $(CLIBS)

ifneq ($(origin BUILD_SRC),file)
BUILD_SRC := $(wildcard *.$(BUILD_LANG))
endif
DEPS := $(BUILD_SRC:%=$(BUILD_DEPS)/%.d)

ifeq ($(BUILD_USE),1)
ifeq ($(VALGRIND),1)
BUILD_FLAGS += -D$(PROJECT_NAME_UP)_TEST_VALGRIND
endif
endif

ifeq ($(STATIC),1)
BUILD_LIB_EXT = a
else
BUILD_LIB_EXT = so
endif

ifeq ($(BUILD_TYPE),$(BUILD_O))
BUILD_SRC := $(BUILD_SRC:%.$(BUILD_LANG)=%.o)
endif

BUILD_NAME_STR_ERROR = You must define BUILD_NAME to use BUILD_TYPE=

ifeq ($(BUILD_TYPE),$(BUILD_LIB))
ifeq ($(BUILD_NAME),)
$(error $(BUILD_NAME_STR_ERROR)$$(BUILD_LIB))
endif
BUILD_SRC := $(BUILD_SRC:%.$(BUILD_LANG)=%.o) lib$(BUILD_NAME).$(BUILD_LIB_EXT)
endif

ifeq ($(BUILD_TYPE),$(BUILD_EXE))
ifeq ($(BUILD_ALL),1)
BUILD_SRC := $(BUILD_SRC:%.$(BUILD_LANG)=%.o)
ifeq ($(BUILD_NAME),)
$(error $(BUILD_NAME_STR_ERROR)$$(BUILD_EXE) and BUILD_ALL=1)
endif
BUILD_SRC += $(BUILD_NAME).exe
else
BUILD_SRC := $(BUILD_SRC:%.$(BUILD_LANG)=%)
endif
endif

_BUILD_FILES := $(BUILD_SRC:%=$(BUILD_OUT)/%)

.PRECIOUS: $(_BUILD_FILES)

.PHONY: do_build
do_build: subdirs $(DEPS) $(_BUILD_FILES)

${BUILD_DEPS} ${BUILD_OUT}:
	$(Q)mkdir -p $@

.PHONY: subdirs
subdirs:
ifdef BUILD_SUBDIRS
	$(Q)set -e; for i in $(BUILD_SUBDIRS); do $(MAKE) -C $$i; done
endif

${BUILD_OUT}/%.o: %.$(BUILD_LANG) | $(BUILD_OUT)
	$(Q)$(BUILD_COMP) $(BUILD_FLAGS) -fPIC -c -o $@ $< $(BUILD_INCLUDES)

${BUILD_OUT}/%: %.$(BUILD_LANG) | $(BUILD_OUT)
	$(Q)$(BUILD_COMP) $(BUILD_FLAGS) -o $@ $< \
		$(BUILD_INCLUDES) $(BUILD_LIBRARIES)
ifeq ($(BUILD_USE),1)
ifeq ($(VALGRIND),1)
	$(Q)LD_LIBRARY_PATH=$(LD_LIBRARY_PATH):$(LIBS_PATH) valgrind \
		--track-origins=yes --leak-check=full --show-leak-kinds=all $@
else
	$(Q)LD_LIBRARY_PATH=$(LD_LIBRARY_PATH):$(LIBS_PATH) $@
endif
endif

.SECONDEXPANSION:

${BUILD_OUT}/%.so: $$(shell find $$(dir $$@) -name *.o) | $(BUILD_OUT)
	$(Q)$(BUILD_COMP) $(BUILD_FLAGS) -o $@ $^ -shared $(BUILD_LIBRARIES)

${BUILD_OUT}/%.a: $$(shell find $$(dir $$@) -name *.o) | $(BUILD_OUT)
	$(Q)$(AR) rsc $@ $^

${BUILD_OUT}/%.exe: $$(shell find $$(dir $$@) -name *.o) | $(BUILD_OUT)
	$(Q)$(BUILD_COMP) $(BUILD_FLAGS) -o $@ $^ $(BUILD_LIBRARIES)
	$(Q)ln -f $@ $(basename $@)

${BUILD_DEPS}/%.d: % | $(BUILD_DEPS)
	$(Q)$(BUILD_COMP) -MM $(BUILD_INCLUDES) -MF $@ \
		-MT $(BUILD_OUT)/$(basename $(notdir $<)).o $< $(BUILD_FLAGS)

include $(DEPS)
