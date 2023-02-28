BUILD_TYPE  := $(BUILD_EXE)
BUILD_FLAGS  = -D__RANDOM__=$$RANDOM
BUILD_INCL  := $(DIR_TOP)/tests/preempt
BUILD_LIBS   = m dl $(PROJECT_NAME) $(PROJECT_NAME)_preempt
BUILD_USE    = 1
BUILD_SRC   := $(filter-out 009_tcp.c, $(wildcard *.c))

include $(DIR_TOP)/Rules.make
