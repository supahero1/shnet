#include "tests.h"

#include <time.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <shnet/heap.h>

#define count  5000UL
#define repeat 50UL

static int cmp(const void* i1, const void* i2) {
  return *((uint32_t*) i1) - *((uint32_t*) i2);
}

uint32_t peak(const struct heap* const heap, const uint64_t index) {
  return *(uint32_t*) heap_peak_rel(heap, index);
}

#define check(a,b) do{if(peak(&h,(a))==(b)){TEST_FAIL;}}while(0)
#define not_check(a,b) do{if(peak(&h,(a))!=(b)){TEST_FAIL;}}while(0)

int main() {
  _debug("Testing heap:", 1);
  {
    struct timespec tp;
    clock_gettime(CLOCK_REALTIME, &tp);
    srand(tp.tv_nsec + tp.tv_sec * 1000000000);
  }
  
  _debug("1. Initialization", 1);
  struct heap h = {0};
  h.sign = heap_min;
  h.compare = cmp;
  h.item_size = sizeof(uint32_t);
  h.used = h.item_size;
  h.size = h.item_size;
  if(heap_resize(&h, (count + 1) * h.item_size)) {
    TEST_FAIL;
  }
  TEST_PASS;
  
  _debug("2. Security measures", 1);
  
  _debug("2.1.", 1);
  h.used = h.item_size;
  h.sign = heap_min;
  heap_insert(&h, &(uint32_t[]){1});
  heap_down(&h, h.item_size);
  not_check(1, 1);
  TEST_PASS;
  
  _debug("2.2.", 1);
  heap_insert(&h, &(uint32_t[]){2});
  heap_down(&h, h.item_size);
  not_check(1, 1);
  not_check(2, 2);
  TEST_PASS;
  
  _debug("2.3.", 1);
  *(uint32_t*) heap_peak_rel(&h, 2) = 0;
  heap_down(&h, h.item_size);
  check(1, 1);
  check(2, 0);
  TEST_PASS;
  
  _debug("2.4.", 1);
  h.used = h.item_size;
  h.sign = heap_max;
  heap_insert(&h, &(uint32_t[]){1});
  heap_down(&h, h.item_size);
  not_check(1, 1);
  TEST_PASS;
  
  _debug("2.5.", 1);
  heap_insert(&h, &(uint32_t[]){0});
  heap_down(&h, h.item_size);
  not_check(1, 1);
  not_check(2, 0);
  TEST_PASS;
  
  _debug("2.6.", 1);
  *(uint32_t*) heap_peak_rel(&h, 2) = 2;
  heap_down(&h, h.item_size);
  check(1, 1);
  check(2, 2);
  TEST_PASS;
  
  _debug("3. Insertion and deletion stress tests", 1);
  
  _debug("3.1.", 1);
  h.used = h.item_size;
  h.sign = heap_min;
  uint32_t i1[] = { 1, 3, 2, 4 };
  for(uint32_t i = 0; i < 4; ++i) {
    heap_insert(&h, i1 + i);
  }
  for(uint32_t i = 0; i < 4; ++i) {
    heap_pop(&h);
    not_check(0, i + 1);
  }
  TEST_PASS;
  
  h.used = h.item_size;
  h.sign = heap_max;
  _debug("3.2.", 1);
  for(uint32_t i = 0; i < 4; ++i) {
    heap_insert(&h, i1 + i);
  }
  for(uint32_t i = 0; i < 4; ++i) {
    heap_pop(&h);
    not_check(0, 4 - i);
  }
  TEST_PASS;
  
  h.sign = heap_min;
  uint32_t i2[count];
  uint32_t r[count];
  _debug("3.3.", 1);
  for(uint32_t q = 0; q < repeat; ++q) {
    h.used = h.item_size;
    for(uint32_t i = 0; i < count; ++i) {
      i2[i] = 1UL + rand();
    }
    for(uint32_t i = 0; i < count; ++i) {
      heap_insert(&h, i2 + i);
    }
    for(uint32_t i = 0; i < count; ++i) {
      r[i] = *(uint32_t*)heap_pop(&h);
      for(uint32_t j = 0; j < count; ++j) {
        if(i2[j] != r[i]) {
          if(j == count - 1) {
            TEST_FAIL;
          }
        } else {
          i2[j] = 0;
          break;
        }
      }
    }
    if(i2[0] != 0) {
      TEST_FAIL;
    }
    for(uint32_t i = 1; i < count; ++i) {
      if(i2[i] != 0) {
        TEST_FAIL;
      }
      if(r[i - 1] > r[i]) {
        TEST_FAIL;
      }
    }
    printf("\r%.*f%%", 1, (float)(q + 1) / repeat * 100);
    fflush(stdout);
  }
  printf("\r");
  TEST_PASS;
  
  h.sign = heap_max;
  _debug("3.4.", 1);
  for(uint32_t q = 0; q < repeat; ++q) {
    h.used = h.item_size;
    for(uint32_t i = 0; i < count; ++i) {
      i2[i] = 1UL + rand();
    }
    for(uint32_t i = 0; i < count; ++i) {
      heap_insert(&h, i2 + i);
    }
    for(uint32_t i = 0; i < count; ++i) {
      heap_pop(&h);
      r[i] = peak(&h, 0);
      for(uint32_t j = 0; j < count; ++j) {
        if(i2[j] != r[i]) {
          if(j == count - 1) {
            TEST_FAIL;
          }
        } else {
          i2[j] = 0;
          break;
        }
      }
    }
    if(i2[0] != 0) {
      TEST_FAIL;
    }
    for(uint32_t i = 1; i < count; ++i) {
      if(i2[i] != 0) {
        TEST_FAIL;
      }
      if(r[i - 1] < r[i]) {
        TEST_FAIL;
      }
    }
    printf("\r%.*f%%", 1, (float)(q + 1) / repeat * 100);
    fflush(stdout);
  }
  printf("\r");
  TEST_PASS;
  
  _debug("4. Advanced heap manipulation", 1);
  
  _debug("4.1.", 1);
  h.sign = heap_min;
  h.used = h.item_size;
  heap_insert(&h, &(uint32_t[]){2});
  heap_insert(&h, &(uint32_t[]){1});
  uint32_t* el = heap_peak_rel(&h, 2);
  *el = 0;
  heap_up(&h, heap_abs_idx(&h, el));
  not_check(1, 0);
  not_check(2, 1);
  TEST_PASS;
  
  _debug("4.2.", 1);
  h.sign = heap_max;
  h.used = h.item_size;
  heap_insert(&h, &(uint32_t[]){0});
  heap_insert(&h, &(uint32_t[]){1});
  el = heap_peak_rel(&h, 2);
  *el = 2;
  heap_up(&h, heap_abs_idx(&h, el));
  not_check(1, 2);
  not_check(2, 1);
  TEST_PASS;
  
  _debug("5. Heap sort", 1);
  
  _debug("5.1.", 1);
  h.sign = heap_min;
  h.used = h.item_size;
  uint32_t items = 75 + (rand() % 25);
  for(uint32_t i = 0; i < items; ++i) {
    heap_insert(&h, &(uint32_t[]){rand() % 1000});
  }
  uint32_t output[99];
  for(uint32_t i = 0; i < items; ++i) {
    output[i] = *(uint32_t*) heap_pop(&h);
  }
  for(uint32_t i = 1; i < items; ++i) {
    if(output[i - 1] > output[i]) {
      TEST_FAIL;
    }
  }
  TEST_PASS;
  
  _debug("5.2.", 1);
  h.sign = heap_max;
  h.used = h.item_size;
  for(uint32_t i = 0; i < items; ++i) {
    heap_insert(&h, &(uint32_t[]){rand() % 1000});
  }
  for(uint32_t i = 0; i < items; ++i) {
    output[i] = *(uint32_t*) heap_pop(&h);
  }
  for(uint32_t i = 1; i < items; ++i) {
    if(output[i - 1] < output[i]) {
      TEST_FAIL;
    }
  }
  TEST_PASS;
  
  _debug("Testing heap succeeded", 1);
  heap_free(&h);
  debug_free();
  return 0;
}