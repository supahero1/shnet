#include "tests.h"

#include <time.h>
#include <stdlib.h>
#include <shnet/flesto.h>

/*static void vf(const struct flesto* const flesto) {
  for(unsigned long i = 0; i <= flesto->last_part; ++i) {
    for(unsigned long j = 0; j < flesto->parts[i].capacity; ++j) {
      if(j == 0) {
        printf("%hhu", flesto->parts[i].contents[0]);
      } else {
        printf(" %hhu", flesto->parts[i].contents[j]);
      }
    }
    printf(" | ");
  }
  printf("\n");
}*/

int main() {
  //TEST_BEGIN;
  struct flesto f = flesto(7, 0);
  (void) f;
  /*int free_space = 1000;
  unsigned char trash[free_space];
  for(int i = 0; i < free_space; ++i) {
    trash[i] = i;
  }
  printf_debug("flesto stress test 1");
  for(int i = 0; i < 10000; ++i) {
    if(rand() % 100 == 1 && f.max_amount_of_parts > 5) {
      if(flesto_resize(&f, f.max_amount_of_parts + (rand() % 10) - 5) == flesto_out_of_memory) {
        TEST_FAIL;
      }
    }
    if(free_space == 0) {
      if(flesto_idx_to_ptr(&f, 0, 0, flesto_amount_of_items(&f), flesto_right, NULL, NULL) != NULL) {
        TEST_FAIL;
      }
      if(flesto_idx_to_ptr(&f, 0, 0, flesto_amount_of_items(&f) - 1, flesto_right, NULL, NULL) == NULL) {
        TEST_FAIL;
      }
      int how_much = rand() % 137;
      free_space += how_much;
      flesto_remove_items(&f, NULL, 0, 0, how_much, 1);
    } else {
      int how_much = (rand() % free_space) + 1;
      free_space -= how_much;
      if(flesto_add_items(&f, trash, (unsigned) how_much, 1) != flesto_success) {
        TEST_FAIL;
      }
    }
  }*/
  //TEST_PASS;
  return 0;
}