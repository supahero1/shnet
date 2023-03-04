# shnet's build system

This guide is for people who want to use shnet's top directory
Make files to build libraries, executables, and object files.

Identify where which files you want to build. Then,
in the same directory, create an "empty" Makefile:

```make
include $(DIR_TOP)/Rules.make
```

That's the backbone.

To get `DIR_TOP` and other variables, and to run that file
at all, you need some Makefile above the one you just created,
in file hierarchy. In that parent Makefile, include the following:

```make
BUILD_SUBDIRS = path/to/the/folder/containing/new/makefile

include $(DIR_TOP)/Rules.make
```

Or, if the root folder is above, put the following code in one of the targets:

```make
	$(Q)$(MAKE) -C <folder_name>
```

Coming back to the initial Makefile. By default, it will gather all files with
a `.c` extension to then be built. You can only build one kind of an extension
in a directory. If you'd like to build C++ files, you can use `BUILD_LANG`:

```make
BUILD_LANG = cpp

include $(DIR_TOP)/Rules.make
```

You can also set it to `cc` if that's the file extension you are
using. Only `c` and `<anything other than c>` are recognized.
Anything other than `c` defaults to C++, however only files
with that specific extension you just set will be chosen.

If you have both C and C++ files in a directory, you will need to split
them into 2 directories, one with only C files, the other with only C++ ones.

If you don't want to include all files in
the build process, you can use `BUILD_SRC`:

```make
BUILD_SRC := $(filter-out unwanted_file.c, $(wildcard *.c))
# or
BUILD_SRC = list.c of.c file.c names.c

include $(DIR_TOP)/Rules.make
```

You can set explicit compilation flags, include
headers, and link libraries that your files require:

```make
BUILD_FLAGS := -Wno-unused-parameter -g3
BUILD_INCL  := $(DIR_TOP)/custom_include /usr/local/include
BUILD_LIBS  := m shnet

include $(DIR_TOP)/Rules.make
```

Any libraries prefixed with `shnet` are available for local
use (these files will reside in the output directory). You
create them with Makefiles too, explained in other section below.

All paths you pass via `BUILD_INCL` will be converted to relative ones.

After this simple environment setup, you need to decide how to
build the files with the given settings. These are your options:

- `BUILD_O` (default) - For each `.$(BUILD_LANG)` file
    within this directory => `.o`,

- `BUILD_LIB` - First, perform `BUILD_O` step. Then, all `.o` files within this
    directory and all subdirectories => `lib$(BUILD_NAME)` + (`.so` or `.a`),
    which one depends on the variable `STATIC` set by user (`.so` by default),
    so that if `BUILD_NAME = shnet`, the output is `libshnet.so`,

- `BUILD_EXE` - If `BUILD_ALL` is 0 (default), for each `filename.$(BUILD_LANG)`
    within this directory => `filename` (executable). Otherwise (`BUILD_ALL=1`),
    first perform `BUILD_O` step, and then all `.$(BUILD_LANG)` files within
    this directory and all subdirectories => `$(BUILD_NAME).exe` (executable),
    which then receives a symbolic link to `$(BUILD_NAME)` (executable).

`BUILD_NAME` is specifically used in `BUILD_LIB` and `BUILD_EXE` on
demand, to know how to name the resulting file. If the build system
requires it, but it's not given, you will receive an error saying so.

These 3 variables listed above are valid values that `BUILD_TYPE` can have.

If `BUILD_EXE` is specified, you can also declare `BUILD_USE = 1`
for the generated files to be executed, useful for tests.

Examples:

- A folder containing C test suites:

    ```make
    BUILD_TYPE  := $(BUILD_EXE)
    BUILD_FLAGS  = -g3
    BUILD_LIBS   = m shnet
    BUILD_USE    = 1

    include $(DIR_TOP)/Rules.make
    ```

- A folder containing source files and
    a library made out of them, except one file:

    ```make
    BUILD_SRC  := $(filter-out black_sheep.c, $(wildcard *.c))
    BUILD_TYPE := $(BUILD_LIB)
    BUILD_NAME  = my_beloved_library

    include $(DIR_TOP)/Rules.make
    ```

- A folder only containing other folders:

    ```make
    BUILD_SUBDIRS = c cpp dyson/extern/c

    include $(DIR_TOP)/Rules.make
    ```

Note that you can include `BUILD_SUBDIRS` even if you are building
things in the current directory. Subdirectories are built first.
