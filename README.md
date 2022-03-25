**Shnet** is an asynchronous networking library created in "modules", each of which can be used by the application independently.

# Requirements

- Any system with a [Linux kernel](https://www.kernel.org/)
- [Clang](https://clang.llvm.org/) or [GCC](https://gcc.gnu.org/)
- Clang++ for C++ support
- [Make](https://www.gnu.org/software/make/)

# Building

The master branch might not always be stable. If you seek production-ready code, get the latest release.

```bash
git clone -b master https://github.com/supahero1/shnet
cd shnet
```

The library is available in static and dynamic releases:

```bash
sudo make static
sudo make dynamic
```

To build without installing:
```bash
sudo make build
```

You can then test the library:
```bash
sudo make test
```

To build tests, but not execute them (for instance to run manually with Valgrind), do:
```bash
sudo make build-tests
```

To remove build files:
```bash
sudo make clean
```

To uninstall the library:
```bash
sudo make uninstall
```

Multiple `make` commands can be chained very simply. The following command performs a full dynamic reinstall and tests the library afterwards:
```bash
sudo make clean uninstall dynamic test
```

To link the library with your project, simply add the `-lshnet` flag.

# C++

Since Clang supports the C `_Atomic` keyword in C++ code, it is possible to use both languages in one project:
```c
extern "C" {
  #include <shnet/header.h>
}

/* ... C++ code ... */
```