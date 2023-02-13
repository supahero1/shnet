BUILD_TYPE  := $(BUILD_EXE)
BUILD_FLAGS  = -D__RANDOM__=$$RANDOM
BUILD_INCL  := $(DIR_TOP)/tests/preempt
BUILD_LIBS   = m dl $(PROJECT_NAME) $(PROJECT_NAME)_preempt
BUILD_USE    = 1
BUILD_SRC    = 001_test.c 002_error.c 003_storage.c \
	004_thread.c 005_threads.c 006_thread_pool.c 007_time.c

include $(DIR_TOP)/Rules.make
