**Shnet** is a multi-purpose library created in "modules",
each of which can be used by the application independently.

# Requirements

Obligatory:

- Any system with the [Linux kernel](https://www.kernel.org/)
- [GCC](https://gcc.gnu.org/)
- [Make](https://www.gnu.org/software/make/)
- ~~[OpenSSL](https://github.com/openssl/openssl)~~ (next release)

[Valgrind](https://valgrind.org/) is necessary
if you want to test the code in debug mode.

See `INSTALL` or do `make help` to learn
how to build, install, and test the library.

**DO NOT** `git clone` this repository. Instead, download
the latest release that is not flagged as a pre-release.

The header files may be embedded within a C++ project, however note
that no C++ code may directly try to access atomic variables in the
library (as in, you would never need to access them anyway, unless
you explicitly tried to).

## Licenses

This project is licensed under Apache License
2.0 and using the following libraries:

- [OpenSSL](https://github.com/openssl/openssl) (Apache License 2.0)
