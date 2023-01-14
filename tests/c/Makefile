BUILD_TYPE  := $(BUILD_EXE)
BUILD_FLAGS  = -Wno-unused-parameter -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=3 \
	-g3 -D__RANDOM__=$$RANDOM
BUILD_INCL  := $(DIR_TOP)/tests/preempt
BUILD_LIBS   = m dl $(PROJECT_NAME) $(PROJECT_NAME)_preempt
BUILD_USE    = 1
BUILD_SRC    = 001_test.c 002_error.c 003_storage.c

include $(DIR_TOP)/Rules.make
