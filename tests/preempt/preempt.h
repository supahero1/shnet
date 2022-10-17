#ifndef _shnet_tests_preempt_h_
#define _shnet_tests_preempt_h_ 1

#ifdef __cplusplus
extern "C" {
#endif

#define _GNU_SOURCE

#include <pthread.h>

struct preemption_data {
  pthread_t* a;
  const pthread_attr_t* b;
  void* (*c)(void*);
  void* d;
};

/*
 * This only prevents new threads from
 * being created, temporarily. It does
 * not actually prevent preemption.
 */

extern void test_preempt_off(void);

extern void test_preempt_on(void);

#ifdef __cplusplus
}
#endif

#endif /* _shnet_tests_preempt_h_ */