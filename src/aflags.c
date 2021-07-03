#include "aflags.h"

/* 32bit */

uint32_t aflag32_get(const _Atomic uint32_t* const aflag) {
  return atomic_load(aflag);
}

void aflag32_add(_Atomic uint32_t* const aflag, const uint32_t flags) {
  (void) atomic_fetch_or(aflag, flags);
}

void aflag32_del(_Atomic uint32_t* const aflag, const uint32_t flags) {
  (void) atomic_fetch_and(aflag, ~flags);
}

uint32_t aflag32_test(const _Atomic uint32_t* const aflag, const uint32_t flags) {
  return atomic_load(aflag) & flags;
}

void aflag32_clear(_Atomic uint32_t* const aflag) {
  atomic_store(aflag, 0);
}

/* 64bit */

uint64_t aflag64_get(const _Atomic uint64_t* const aflag) {
  return atomic_load(aflag);
}

void aflag64_add(_Atomic uint64_t* const aflag, const uint64_t flags) {
  (void) atomic_fetch_or(aflag, flags);
}

void aflag64_del(_Atomic uint64_t* const aflag, const uint64_t flags) {
  (void) atomic_fetch_and(aflag, ~flags);
}

uint64_t aflag64_test(const _Atomic uint64_t* const aflag, const uint64_t flags) {
  return atomic_load(aflag) & flags;
}

void aflag64_clear(_Atomic uint64_t* const aflag) {
  atomic_store(aflag, 0);
}