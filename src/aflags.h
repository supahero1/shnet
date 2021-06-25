#ifndef Fnt2IjA_eEYD_c1Nk__Gz_PDSJu_KcGf
#define Fnt2IjA_eEYD_c1Nk__Gz_PDSJu_KcGf 1

#include <stdint.h>
#include <stdatomic.h>

extern uint32_t aflag32_get(const _Atomic uint32_t* const);

extern void aflag32_add(_Atomic uint32_t* const, const uint32_t);

extern void aflag32_del(_Atomic uint32_t* const, const uint32_t);

extern uint32_t aflag32_test(const _Atomic uint32_t* const, const uint32_t);

extern void aflag32_clear(_Atomic uint32_t* const);


extern uint64_t aflag64_get(const _Atomic uint64_t* const);

extern void aflag64_add(_Atomic uint64_t* const, const uint64_t);

extern void aflag64_del(_Atomic uint64_t* const, const uint64_t);

extern uint64_t aflag64_test(const _Atomic uint64_t* const, const uint64_t);

extern void aflag64_clear(_Atomic uint64_t* const);

#endif // Fnt2IjA_eEYD_c1Nk__Gz_PDSJu_KcGf