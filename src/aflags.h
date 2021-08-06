#ifndef Fnt2IjA_eEYD_c1Nk__Gz_PDSJu_KcGf
#define Fnt2IjA_eEYD_c1Nk__Gz_PDSJu_KcGf 1

#include <stdint.h>
#include <stdatomic.h>

extern uint8_t aflag8_get(const _Atomic uint8_t* const);

extern void aflag8_add(_Atomic uint8_t* const, const uint8_t);

extern void aflag8_del(_Atomic uint8_t* const, const uint8_t);

extern uint8_t aflag8_test(const _Atomic uint8_t* const, const uint8_t);

extern void aflag8_clear(_Atomic uint8_t* const);


extern uint8_t aflag8_get2(const _Atomic uint8_t* const);

extern void aflag8_add2(_Atomic uint8_t* const, const uint8_t);

extern void aflag8_del2(_Atomic uint8_t* const, const uint8_t);

extern uint8_t aflag8_test2(const _Atomic uint8_t* const, const uint8_t);

extern void aflag8_clear2(_Atomic uint8_t* const);


extern uint16_t aflag16_get(const _Atomic uint16_t* const);

extern void aflag16_add(_Atomic uint16_t* const, const uint16_t);

extern void aflag16_del(_Atomic uint16_t* const, const uint16_t);

extern uint16_t aflag16_test(const _Atomic uint16_t* const, const uint16_t);

extern void aflag16_clear(_Atomic uint16_t* const);


extern uint16_t aflag16_get2(const _Atomic uint16_t* const);

extern void aflag16_add2(_Atomic uint16_t* const, const uint16_t);

extern void aflag16_del2(_Atomic uint16_t* const, const uint16_t);

extern uint16_t aflag16_test2(const _Atomic uint16_t* const, const uint16_t);

extern void aflag16_clear2(_Atomic uint16_t* const);


extern uint32_t aflag32_get(const _Atomic uint32_t* const);

extern void aflag32_add(_Atomic uint32_t* const, const uint32_t);

extern void aflag32_del(_Atomic uint32_t* const, const uint32_t);

extern uint32_t aflag32_test(const _Atomic uint32_t* const, const uint32_t);

extern void aflag32_clear(_Atomic uint32_t* const);


extern uint32_t aflag32_get2(const _Atomic uint32_t* const);

extern void aflag32_add2(_Atomic uint32_t* const, const uint32_t);

extern void aflag32_del2(_Atomic uint32_t* const, const uint32_t);

extern uint32_t aflag32_test2(const _Atomic uint32_t* const, const uint32_t);

extern void aflag32_clear2(_Atomic uint32_t* const);


extern uint64_t aflag64_get(const _Atomic uint64_t* const);

extern void aflag64_add(_Atomic uint64_t* const, const uint64_t);

extern void aflag64_del(_Atomic uint64_t* const, const uint64_t);

extern uint64_t aflag64_test(const _Atomic uint64_t* const, const uint64_t);

extern void aflag64_clear(_Atomic uint64_t* const);


extern uint64_t aflag64_get2(const _Atomic uint64_t* const);

extern void aflag64_add2(_Atomic uint64_t* const, const uint64_t);

extern void aflag64_del2(_Atomic uint64_t* const, const uint64_t);

extern uint64_t aflag64_test2(const _Atomic uint64_t* const, const uint64_t);

extern void aflag64_clear2(_Atomic uint64_t* const);


/* Generic sequentially consistent */
#define aflag_get(a) _Generic((a), \
_Atomic uint8_t*: aflag8_get, \
_Atomic const uint8_t*: aflag8_get, \
_Atomic uint16_t*: aflag16_get, \
_Atomic const uint16_t*: aflag16_get, \
_Atomic uint32_t*: aflag32_get, \
_Atomic const uint32_t*: aflag32_get, \
_Atomic uint64_t*: aflag64_get, \
_Atomic const uint64_t*: aflag64_get)(a)
#define aflag_add(a,b) _Generic((a), \
_Atomic uint8_t*: aflag8_add, \
_Atomic const uint8_t*: aflag8_add, \
_Atomic uint16_t*: aflag16_add, \
_Atomic const uint16_t*: aflag16_add, \
_Atomic uint32_t*: aflag32_add, \
_Atomic const uint32_t*: aflag32_add, \
_Atomic uint64_t*: aflag64_add, \
_Atomic const uint64_t*: aflag64_add)(a,b)
#define aflag_del(a,b) _Generic((a), \
_Atomic uint8_t*: aflag8_del, \
_Atomic const uint8_t*: aflag8_del, \
_Atomic uint16_t*: aflag16_del, \
_Atomic const uint16_t*: aflag16_del, \
_Atomic uint32_t*: aflag32_del, \
_Atomic const uint32_t*: aflag32_del, \
_Atomic uint64_t*: aflag64_del, \
_Atomic const uint64_t*: aflag64_del)(a,b)
#define aflag_test(a,b) _Generic((a), \
_Atomic uint8_t*: aflag8_test, \
_Atomic const uint8_t*: aflag8_test, \
_Atomic uint16_t*: aflag16_test, \
_Atomic const uint16_t*: aflag16_test, \
_Atomic uint32_t*: aflag32_test, \
_Atomic const uint32_t*: aflag32_test, \
_Atomic uint64_t*: aflag64_test, \
_Atomic const uint64_t*: aflag64_test)(a,b)
#define aflag_clear(a) _Generic((a), \
_Atomic uint8_t*: aflag8_clear, \
_Atomic const uint8_t*: aflag8_clear, \
_Atomic uint16_t*: aflag16_clear, \
_Atomic const uint16_t*: aflag16_clear, \
_Atomic uint32_t*: aflag32_clear, \
_Atomic const uint32_t*: aflag32_clear, \
_Atomic uint64_t*: aflag64_clear, \
_Atomic const uint64_t*: aflag64_clear)(a)
/* Generic release acquire */
#define aflag_get2(a) _Generic((a), \
_Atomic uint8_t*: aflag8_get2, \
_Atomic const uint8_t*: aflag8_get2, \
_Atomic uint16_t*: aflag16_get2, \
_Atomic const uint16_t*: aflag16_get2, \
_Atomic uint32_t*: aflag32_get2, \
_Atomic const uint32_t*: aflag32_get2, \
_Atomic uint64_t*: aflag64_get2, \
_Atomic const uint64_t*: aflag64_get2)(a)
#define aflag_add2(a,b) _Generic((a), \
_Atomic uint8_t*: aflag8_add2, \
_Atomic const uint8_t*: aflag8_add2, \
_Atomic uint16_t*: aflag16_add2, \
_Atomic const uint16_t*: aflag16_add2, \
_Atomic uint32_t*: aflag32_add2, \
_Atomic const uint32_t*: aflag32_add2, \
_Atomic uint64_t*: aflag64_add2, \
_Atomic const uint64_t*: aflag64_add2)(a,b)
#define aflag_del2(a,b) _Generic((a), \
_Atomic uint8_t*: aflag8_del2, \
_Atomic const uint8_t*: aflag8_del2, \
_Atomic uint16_t*: aflag16_del2, \
_Atomic const uint16_t*: aflag16_del2, \
_Atomic uint32_t*: aflag32_del2, \
_Atomic const uint32_t*: aflag32_del2, \
_Atomic uint64_t*: aflag64_del2, \
_Atomic const uint64_t*: aflag64_del2)(a,b)
#define aflag_test2(a,b) _Generic((a), \
_Atomic uint8_t*: aflag8_test2, \
_Atomic const uint8_t*: aflag8_test2, \
_Atomic uint16_t*: aflag16_test2, \
_Atomic const uint16_t*: aflag16_test2, \
_Atomic uint32_t*: aflag32_test2, \
_Atomic const uint32_t*: aflag32_test2, \
_Atomic uint64_t*: aflag64_test2, \
_Atomic const uint64_t*: aflag64_test2)(a,b)
#define aflag_clear2(a) _Generic((a), \
_Atomic uint8_t*: aflag8_clear2, \
_Atomic const uint8_t*: aflag8_clear2, \
_Atomic uint16_t*: aflag16_clear2, \
_Atomic const uint16_t*: aflag16_clear2, \
_Atomic uint32_t*: aflag32_clear2, \
_Atomic const uint32_t*: aflag32_clear2, \
_Atomic uint64_t*: aflag64_clear2, \
_Atomic const uint64_t*: aflag64_clear2)(a)

#endif // Fnt2IjA_eEYD_c1Nk__Gz_PDSJu_KcGf