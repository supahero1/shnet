VERSION = 1
PATCH = 3

CLI_VERSION = 1
CLI_PATCH = 0

PROJECT_NAME = shnet
PROJECT_NAME_UP = $(shell echo "$(PROJECT_NAME)" | tr "[:lower:]" "[:upper:]")

SHELL = bash

FMT_OD :=@printf "$(shell tput bold)
FMT_DO :=$(shell tput sgr0)\n"

# DIR
DIR_TOP     := $(shell pwd)
DIR_OUT     ?= bin
DIR_INCLUDE ?= /usr/local/include
DIR_LIB     ?= /usr/local/lib
DIR_BIN     ?= /usr/local/bin

# BUILD_TYPE
BUILD_O   = 1
BUILD_LIB = 2
BUILD_EXE = 3

ifneq ($(VERBOSE),1)
Q = @
COVFLAGS = -q
endif

BASE_FLAGS = -Wall -Wextra -flto -O3

ifeq ($(DEBUG),1)
BASE_FLAGS += -U_FORTIFY_SOURCE -D_FORTIFY_SOURCE=3 -g3
endif

ifeq ($(COVERAGE),1)
BASE_FLAGS += --coverage
endif

CLIBS = -pthread#-lssl -lcrypto

ifeq ($(WITH_LIBUV),1)
BASE_FLAGS += -DLIBUV
CLIBS  += -luv
endif

CFLAGS   = $(BASE_FLAGS)
CXXFLAGS = $(BASE_FLAGS)
