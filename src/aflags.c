#include "aflags.h"

/* 8bit sequentially consistent */

uint8_t aflag8_get(const _Atomic uint8_t* const aflag) {
  return atomic_load(aflag);
}

void aflag8_add(_Atomic uint8_t* const aflag, const uint8_t flags) {
  (void) atomic_fetch_or(aflag, flags);
}

void aflag8_del(_Atomic uint8_t* const aflag, const uint8_t flags) {
  (void) atomic_fetch_and(aflag, ~flags);
}

uint8_t aflag8_test(const _Atomic uint8_t* const aflag, const uint8_t flags) {
  return atomic_load(aflag) & flags;
}

void aflag8_clear(_Atomic uint8_t* const aflag) {
  atomic_store(aflag, 0);
}

/* 8bit release acquire */

uint8_t aflag8_get2(const _Atomic uint8_t* const aflag) {
  return atomic_load_explicit(aflag, memory_order_acquire);
}

void aflag8_add2(_Atomic uint8_t* const aflag, const uint8_t flags) {
  (void) atomic_fetch_or_explicit(aflag, flags, memory_order_acq_rel);
}

void aflag8_del2(_Atomic uint8_t* const aflag, const uint8_t flags) {
  (void) atomic_fetch_and_explicit(aflag, ~flags, memory_order_acq_rel);
}

uint8_t aflag8_test2(const _Atomic uint8_t* const aflag, const uint8_t flags) {
  return atomic_load_explicit(aflag, memory_order_acquire) & flags;
}

void aflag8_clear2(_Atomic uint8_t* const aflag) {
  atomic_store_explicit(aflag, 0, memory_order_release);
}

/* 16bit sequentially consistent */

uint16_t aflag16_get(const _Atomic uint16_t* const aflag) {
  return atomic_load(aflag);
}

void aflag16_add(_Atomic uint16_t* const aflag, const uint16_t flags) {
  (void) atomic_fetch_or(aflag, flags);
}

void aflag16_del(_Atomic uint16_t* const aflag, const uint16_t flags) {
  (void) atomic_fetch_and(aflag, ~flags);
}

uint16_t aflag16_test(const _Atomic uint16_t* const aflag, const uint16_t flags) {
  return atomic_load(aflag) & flags;
}

void aflag16_clear(_Atomic uint16_t* const aflag) {
  atomic_store(aflag, 0);
}

/* 16bit release acquire */

uint16_t aflag16_get2(const _Atomic uint16_t* const aflag) {
  return atomic_load_explicit(aflag, memory_order_acquire);
}

void aflag16_add2(_Atomic uint16_t* const aflag, const uint16_t flags) {
  (void) atomic_fetch_or_explicit(aflag, flags, memory_order_acq_rel);
}

void aflag16_del2(_Atomic uint16_t* const aflag, const uint16_t flags) {
  (void) atomic_fetch_and_explicit(aflag, ~flags, memory_order_acq_rel);
}

uint16_t aflag16_test2(const _Atomic uint16_t* const aflag, const uint16_t flags) {
  return atomic_load_explicit(aflag, memory_order_acquire) & flags;
}

void aflag16_clear2(_Atomic uint16_t* const aflag) {
  atomic_store_explicit(aflag, 0, memory_order_release);
}

/* 32bit sequentially consistent */

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

/* 32bit release acquire */

uint32_t aflag32_get2(const _Atomic uint32_t* const aflag) {
  return atomic_load_explicit(aflag, memory_order_acquire);
}

void aflag32_add2(_Atomic uint32_t* const aflag, const uint32_t flags) {
  (void) atomic_fetch_or_explicit(aflag, flags, memory_order_acq_rel);
}

void aflag32_del2(_Atomic uint32_t* const aflag, const uint32_t flags) {
  (void) atomic_fetch_and_explicit(aflag, ~flags, memory_order_acq_rel);
}

uint32_t aflag32_test2(const _Atomic uint32_t* const aflag, const uint32_t flags) {
  return atomic_load_explicit(aflag, memory_order_acquire) & flags;
}

void aflag32_clear2(_Atomic uint32_t* const aflag) {
  atomic_store_explicit(aflag, 0, memory_order_release);
}

/* 64bit sequentially consistent */

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

/* 64bit release acquire */

uint64_t aflag64_get2(const _Atomic uint64_t* const aflag) {
  return atomic_load_explicit(aflag, memory_order_acquire);
}

void aflag64_add2(_Atomic uint64_t* const aflag, const uint64_t flags) {
  (void) atomic_fetch_or_explicit(aflag, flags, memory_order_acq_rel);
}

void aflag64_del2(_Atomic uint64_t* const aflag, const uint64_t flags) {
  (void) atomic_fetch_and_explicit(aflag, ~flags, memory_order_acq_rel);
}

uint64_t aflag64_test2(const _Atomic uint64_t* const aflag, const uint64_t flags) {
  return atomic_load_explicit(aflag, memory_order_acquire) & flags;
}

void aflag64_clear2(_Atomic uint64_t* const aflag) {
  atomic_store_explicit(aflag, 0, memory_order_release);
}