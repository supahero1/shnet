# Command-line interface

This directory contains the WIP version of the shnet CLI.

Currently, only `time-bench` method is supported. In the future, more TCP
and TLS related methods will be added with many settings to customize.

## Important notes

- If you want to have `shnet time-bench` test against Libuv, build the library
  with `make WITH_LIBUV=1`. If you had already built the library, clean it first
  with `make clean` before rebuilding with different flags.
