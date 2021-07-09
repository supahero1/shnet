#include "misc.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

int do_async(void* (*func)(void*), void* const data) {
  pthread_t t;
  pthread_attr_t attr;
  int err = pthread_attr_init(&attr);
  if(err != 0) {
    return err;
  }
  (void) pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
  err = pthread_create(&t, &attr, func, data);
  (void) pthread_attr_destroy(&attr);
  return err;
}

int do_joinable_async(void* (*func)(void*), void* const data, pthread_t* const id) {
  return pthread_create(id, NULL, func, data);
}