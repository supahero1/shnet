BUILD_TYPE  := $(BUILD_EXE)
BUILD_FLAGS  = -Wno-unused-parameter -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=3 \
	-g3 -D__RANDOM__=$$RANDOM
BUILD_INCL  := $(DIR_TOP)/tests/preempt
BUILD_LIBS   = $(PROJECT_NAME)
BUILD_USE    = 1
BUILD_LANG   = cpp

include $(DIR_TOP)/Rules.make
