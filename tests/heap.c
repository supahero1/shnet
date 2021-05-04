#include "tests.h"

#include <time.h>
#include <stdlib.h>
#include <string.h>
#include <shnet/heap.h>

#define count  10000UL
#define repeat 100UL

struct heap_node {
  unsigned long value;
};

static long cmp(const void* i1, const void* i2) {
  const unsigned long m1 = *((unsigned long*) i1);
  const unsigned long m2 = *((unsigned long*) i2);
  if(m1 > m2) {
    return 1;
  } else if(m1 < m2) {
    return -1;
  } else {
    return 0;
  }
}

int main() {
  printf_debug("Testing heap.c:", 1);
  {
    struct timespec tp;
    (void) clock_gettime(CLOCK_REALTIME, &tp);
    srand(tp.tv_nsec + tp.tv_sec * 1000000000);
  }
  printf_debug("1. Initialisation", 1);
  struct heap h = heap(sizeof(struct heap_node), heap_min, cmp);
  if(h.item_size != sizeof(struct heap_node)) {
    TEST_FAIL;
  }
  if(h.used != h.item_size) {
    TEST_FAIL;
  }
  if(h.size != h.used) {
    TEST_FAIL;
  }
  if(h.array != NULL) {
    TEST_FAIL;
  }
  if(h.sign != heap_min) {
    TEST_FAIL;
  }
  if(h.compare != cmp) {
    TEST_FAIL;
  }
  int err = heap_resize(&h, (count + 1) * sizeof(struct heap_node));
  if(err != heap_success) {
    printf_debug("can't allocate mem", 1);
    TEST_FAIL;
  }
  TEST_PASS;
  printf_debug("2. Security measures", 1);
  printf_debug("2.1.", 1);
  h.used = h.item_size;
  h.sign = heap_min;
  (void) heap_add_item(&h, &((struct heap_node) {
    .value = 1
  }));
  heap_down(&h, h.item_size);
  if(((unsigned long*) h.array)[1] != 1) {
    TEST_FAIL;
  }
  TEST_PASS;
  printf_debug("2.2.", 1);
  (void) heap_add_item(&h, &((struct heap_node) {
    .value = 2
  }));
  heap_down(&h, h.item_size);
  if(((unsigned long*) h.array)[1] != 1) {
    TEST_FAIL;
  }
  if(((unsigned long*) h.array)[2] != 2) {
    TEST_FAIL;
  }
  TEST_PASS;
  printf_debug("2.3.", 1);
  ((unsigned long*) h.array)[2] = 0;
  heap_down(&h, h.item_size);
  if(((unsigned long*) h.array)[1] == 1) {
    TEST_FAIL;
  }
  if(((unsigned long*) h.array)[2] == 0) {
    TEST_FAIL;
  }
  TEST_PASS;
  printf_debug("2.4.", 1);
  h.used = h.item_size;
  h.sign = heap_max;
  (void) heap_add_item(&h, &((struct heap_node) {
    .value = 1
  }));
  heap_down(&h, h.item_size);
  if(((unsigned long*) h.array)[1] != 1) {
    TEST_FAIL;
  }
  TEST_PASS;
  printf_debug("2.5.", 1);
  (void) heap_add_item(&h, &((struct heap_node) {
    .value = 0
  }));
  heap_down(&h, h.item_size);
  if(((unsigned long*) h.array)[1] != 1) {
    TEST_FAIL;
  }
  if(((unsigned long*) h.array)[2] != 0) {
    TEST_FAIL;
  }
  TEST_PASS;
  printf_debug("2.6.", 1);
  ((unsigned long*) h.array)[2] = 2;
  heap_down(&h, h.item_size);
  if(((unsigned long*) h.array)[1] == 1) {
    TEST_FAIL;
  }
  if(((unsigned long*) h.array)[2] == 2) {
    TEST_FAIL;
  }
  TEST_PASS;
  printf_debug("3. Insertion and deletion stress tests", 1);
  printf_debug("3.1.", 1);
  h.used = h.item_size;
  h.sign = heap_min;
  unsigned long i1[] = { 1, 3, 2, 4 };
  for(unsigned long i = 0; i < 4; ++i) {
    (void) heap_add_item(&h, &((struct heap_node) {
      .value = i1[i]
    }));
  }
  for(unsigned long i = 0; i < 4; ++i) {
    heap_pop(&h);
    if(((unsigned long*) h.array)[0] != i + 1) {
      TEST_FAIL;
    }
  }
  TEST_PASS;
  h.used = h.item_size;
  h.sign = heap_max;
  printf_debug("3.2.", 1);
  for(unsigned long i = 0; i < 4; ++i) {
    (void) heap_add_item(&h, &((struct heap_node) {
      .value = i1[i]
    }));
  }
  for(unsigned long i = 0; i < 4; ++i) {
    heap_pop(&h);
    if(((unsigned long*) h.array)[0] != 4 - i) {
      TEST_FAIL;
    }
  }
  TEST_PASS;
  h.sign = heap_min;
  unsigned long i2[count];
  unsigned long r[count];
  printf_debug("3.3.", 1);
  for(unsigned long q = 0; q < repeat; ++q) {
    h.used = h.item_size;
    for(unsigned long i = 0; i < count; ++i) {
      i2[i] = 1UL + rand();
    }
    for(unsigned long i = 0; i < count; ++i) {
      (void) heap_add_item(&h, &((struct heap_node) {
        .value = i2[i]
      }));
    }
    for(unsigned long i = 0; i < count; ++i) {
      heap_pop(&h);
      r[i] = ((unsigned long*) h.array)[0];
      for(unsigned long j = 0; j < count; ++j) {
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
    for(unsigned long i = 1; i < count; ++i) {
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
  printf_debug("3.4.", 1);
  for(unsigned long q = 0; q < repeat; ++q) {
    h.used = h.item_size;
    for(unsigned long i = 0; i < count; ++i) {
      i2[i] = 1UL + rand();
    }
    for(unsigned long i = 0; i < count; ++i) {
      (void) heap_add_item(&h, &((struct heap_node) {
        .value = i2[i]
      }));
    }
    for(unsigned long i = 0; i < count; ++i) {
      heap_pop(&h);
      r[i] = ((unsigned long*) h.array)[0];
      for(unsigned long j = 0; j < count; ++j) {
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
    for(unsigned long i = 1; i < count; ++i) {
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
  printf_debug("Testing heap.c succeeded", 1);
  return 0;
}