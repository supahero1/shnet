VERSION = 1
PATCH = 3

CLI_VERSION = 1
CLI_PATCH = 0

SHELL  := bash

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

# BUILD_GROUP
BUILD_ALL  = 1
BUILD_EACH = 2
