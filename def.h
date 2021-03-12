/*
   Copyright 2021 shädam

   Licensed under the Apache License, Version 2.0 (the "License");
   you may not use this file except in compliance with the License.
   You may obtain a copy of the License at

       http://www.apache.org/licenses/LICENSE-2.0

   Unless required by applicable law or agreed to in writing, software
   distributed under the License is distributed on an "AS IS" BASIS,
   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
   See the License for the specific language governing permissions and
   limitations under the License.
*/

#ifndef Q2W0nh8HsNz7b8t0apZPrR4mGpD__m__
#define Q2W0nh8HsNz7b8t0apZPrR4mGpD__m__ 1

#ifndef likely
#define likely(x)                                                               __builtin_expect(!!(x), 1)
#endif

#ifndef probably_likely
#define probably_likely(x, y)                                                   __builtin_expect_with_probability(!!(x), 1, y)
#endif

#ifndef unlikely
#define unlikely(x)                                                             __builtin_expect(!!(x), 0)
#endif

#ifndef probably_unlikely
#define probably_unlikely(x, y)                                                 __builtin_expect_with_probability(!!(x), 0, y)
#endif

#ifndef defvector
#define defvector(type, name, size)                                             typedef type name __attribute__((vector_size(size)))
#endif

#ifndef convertvector
#define convertvector(x, y)                                                     __builtin_convertvector(x, y)
#endif

#ifndef __unreachable
#define __unreachable                                                           __builtin_unreachable()
#endif

#ifndef __const
#define __const                                                                 __attribute__((const))
#endif

#ifndef __pure
#define __pure                                                                  __attribute__((pure))
#endif

#ifndef __nonnull
#define __nonnull(x)                                                            __attribute__((nonnull x))
#endif

#ifndef __noinline
#define __noinline                                                              __attribute__((noinline))
#endif

#ifndef __weak
#define __weak                                                                  __attribute__((weak))
#endif

#ifndef __nothrow
#define __nothrow                                                               __attribute__((nothrow))
#endif

#ifndef __hot
#define __hot                                                                   __attribute__((hot))
#endif

#ifndef __cold
#define __cold                                                                  __attribute__((cold))
#endif

#ifndef __unused
#define __unused                                                                __attribute__((unused))
#endif

#ifndef __used
#define __used                                                                  __attribute__((used))
#endif

#ifndef __noalias
#define __noalias                                                               __attribute__((malloc))
#endif

#ifndef __noreturn
#define __noreturn                                                              __attribute__((noreturn))
#endif

#ifndef __ret_nonnull
#define __ret_nonnull                                                           __attribute__((returns_nonnull))
#endif

#ifndef __ret_unused
#define __ret_unused                                                            __attribute__((warn_unused_result))
#endif

#ifndef __ret_align
#define __ret_align(x)                                                          __attribute__((alloc_align x))
#endif

#ifndef __ret_size
#define __ret_size(x)                                                           __attribute__((alloc_size x))
#endif

#ifndef __align
#define __align(x)                                                              __attribute__((aligned x))
#endif

#ifndef __aligned
#define __aligned                                                               __attribute__((aligned))
#endif

#ifndef __assume_align
#define __assume_align(...)                                                     __builtin_assume_aligned(__VA_ARGS__)
#endif

#ifndef __pack
#define __pack(x)                                                               __attribute__((packed x))
#endif

#ifndef compiler_barrier
#define compiler_barrier                                                        asm volatile("" ::: "memory")
#endif


#include <stddef.h>
#include <stdint.h>

#include <stdio.h>

inline __nothrow __nonnull((1))
void prefetch0(void* addr, size_t length) {
  addr = (void*)((uintptr_t) addr ^ ((uintptr_t) addr & 63U));
  length += -length & 63U;
  __builtin_prefetch(addr, 0, 0);
  for(size_t i = 64U; i < length; i += 64U) {
    __builtin_prefetch(addr + i, 0, 0);
  }
}

inline __nothrow __nonnull((1))
void prefetch1(void* addr, size_t length) {
  addr = (void*)((uintptr_t) addr ^ ((uintptr_t) addr & 63U));
  length += -length & 63U;
  __builtin_prefetch(addr, 0, 1);
  for(size_t i = 64U; i < length; i += 64U) {
    __builtin_prefetch(addr + i, 0, 1);
  }
}

inline __nothrow __nonnull((1))
void prefetch2(void* addr, size_t length) {
  addr = (void*)((uintptr_t) addr ^ ((uintptr_t) addr & 63U));
  length += -length & 63U;
  __builtin_prefetch(addr, 0, 2);
  for(size_t i = 64U; i < length; i += 64U) {
    __builtin_prefetch(addr + i, 0, 2);
  }
}

inline __nothrow __nonnull((1))
void prefetch3(void* addr, size_t length) {
  addr = (void*)((uintptr_t) addr ^ ((uintptr_t) addr & 63U));
  length += -length & 63U;
  __builtin_prefetch(addr, 0, 3);
  for(size_t i = 64U; i < length; i += 64U) {
    __builtin_prefetch(addr + i, 0, 3);
  }
}



inline __nothrow __nonnull((1))
void strict_prefetch0(void* addr, size_t length) {
  addr = (void*)((uintptr_t) addr ^ ((uintptr_t) addr & 63U));
  length += -length & 63U;
  __builtin_prefetch(addr, 1, 0);
  for(size_t i = 64U; i < length; i += 64U) {
    __builtin_prefetch(addr + i, 1, 0);
  }
}

inline __nothrow __nonnull((1))
void strict_prefetch1(void* addr, size_t length) {
  addr = (void*)((uintptr_t) addr ^ ((uintptr_t) addr & 63U));
  length += -length & 63U;
  __builtin_prefetch(addr, 1, 1);
  for(size_t i = 64U; i < length; i += 64U) {
    __builtin_prefetch(addr + i, 1, 1);
  }
}

inline __nothrow __nonnull((1))
void strict_prefetch2(void* addr, size_t length) {
  addr = (void*)((uintptr_t) addr ^ ((uintptr_t) addr & 63U));
  length += -length & 63U;
  __builtin_prefetch(addr, 1, 2);
  for(size_t i = 64U; i < length; i += 64U) {
    __builtin_prefetch(addr + i, 1, 2);
  }
}

inline __nothrow __nonnull((1))
void strict_prefetch3(void* addr, size_t length) {
  addr = (void*)((uintptr_t) addr ^ ((uintptr_t) addr & 63U));
  length += -length & 63U;
  __builtin_prefetch(addr, 1, 3);
  for(size_t i = 64U; i < length; i += 64U) {
    __builtin_prefetch(addr + i, 1, 3);
  }
}



/*#include <stdlib.h>
inline __nothrow __noalias __ret_align((2)) __ret_size((1)) __ret_unused
void* aligned_malloc(const size_t size, const size_t alignment) {
  void* ptr = NULL;
  (void) posix_memalign(&ptr, alignment, size);
  return ptr;
}*/

#endif // Q2W0nh8HsNz7b8t0apZPrR4mGpD__m__