#include "tests.h"
#include "../src/flesto.h"

#define ERR NET_LOG("E %d", __LINE__);\
return 0;

int main() {
  NET_LOG("TEST: flesto.c");
  struct flesto f;
  for(int i = 0; i < 3; ++i) {
    NET_LOG("loop %d", i);
    f = flesto(32, 0);
    if(flesto_resize(&f, 5) != flesto_success) {
      ERR
    }
    if(f.max_amount_of_parts != 5) {
      ERR
    }
    int sum = 0;
    for(int i = 0; i < 5; ++i) {
      for(int j = 0; j < 32; ++j) {
        if(f.parts[i].contents == NULL) {
          ERR
        } else if(f.parts[i].capacity != 0) {
          ERR
        } else if(f.parts[i].max_capacity != 32) {
          ERR
        }
        sum += f.parts[i].contents[j];
      }
    }
    if(flesto_resize(&f, 3) != flesto_success) {
      ERR
    }
    if(flesto_resize(&f, 7) != flesto_success) {
      ERR
    }
    if(flesto_resize(&f, 4) != flesto_success) {
      ERR
    }
    if(flesto_resize(&f, 10) != flesto_success) {
      ERR
    }
    if(flesto_resize(&f, 5) != flesto_success) {
      ERR
    }
    if(f.max_amount_of_parts != 5) {
      ERR
    }
    for(int i = 0; i < 5; ++i) {
      for(int j = 0; j < 32; ++j) {
        if(f.parts[i].contents == NULL) {
          ERR
        } else if(f.parts[i].capacity != 0) {
          ERR
        } else if(f.parts[i].max_capacity != 32) {
          ERR
        }
        sum += f.parts[i].contents[j];
      }
    }
    char* ptr = malloc(32);
    if(ptr == NULL) {
      ERR
    }
    if(flesto_add_part(&f, ptr, 32) != flesto_success) {
      ERR
    }
    if(f.max_amount_of_parts != 6) {
      ERR
    }
    for(int i = 0; i < 5; ++i) {
      for(int j = 0; j < 32; ++j) {
        if(f.parts[i].contents == NULL) {
          ERR
        } else if(f.parts[i].capacity != 0) {
          ERR
        } else if(f.parts[i].max_capacity != 32) {
          ERR
        }
        sum += f.parts[i].contents[j];
      }
    }
    if(flesto_resize(&f, 3) != flesto_success) {
      ERR
    }
    if(flesto_resize(&f, 7) != flesto_success) {
      ERR
    }
    if(flesto_resize(&f, 4) != flesto_success) {
      ERR
    }
    if(flesto_resize(&f, 10) != flesto_success) {
      ERR
    }
    if(flesto_resize(&f, 6) != flesto_success) {
      ERR
    }
    if(f.max_amount_of_parts != 6) {
      ERR
    }
    for(int i = 0; i < 6; ++i) {
      for(int j = 0; j < 32; ++j) {
        if(f.parts[i].contents == NULL) {
          ERR
        } else if(f.parts[i].capacity != 0) {
          ERR
        } else if(f.parts[i].max_capacity != 32) {
          ERR
        }
        sum += f.parts[i].contents[j];
      }
    }
    flesto_free(&f);
    f = flesto(3, 1);
    if(flesto_resize(&f, 6) != flesto_success) {
      ERR
    }
    if(f.max_amount_of_parts != 6) {
      ERR
    }
    if(flesto_check_address3(&f, f.parts[0].contents - 1) != -1) {
      ERR
    }
    if(flesto_check_address3(&f, f.parts[0].contents    ) == -1) {
      ERR
    }
    if(flesto_check_address3(&f, f.parts[0].contents + 1) == -1) {
      ERR
    }
    if(flesto_check_address3(&f, f.parts[3].contents - 1) != -1) {
      ERR
    }
    if(flesto_check_address3(&f, f.parts[3].contents    ) == -1) {
      ERR
    }
    if(flesto_check_address3(&f, f.parts[3].contents + 1) == -1) {
      ERR
    }
    if(flesto_check_address3(&f, f.parts[5].contents - 1) != -1) {
      ERR
    }
    if(flesto_check_address3(&f, f.parts[5].contents    ) == -1) {
      ERR
    }
    if(flesto_check_address3(&f, f.parts[5].contents + 1) == -1) {
      ERR
    }
    if(flesto_check_address3(&f, f.parts[5].contents + 5) != -1) {
      ERR
    }
    if(flesto_check_address3(&f, f.parts[5].contents + 3) != -1) {
      ERR
    }
    if(flesto_check_address3(&f, f.parts[5].contents + 2) == -1) {
      ERR
    }
    flesto_free(&f);
  }
  f = flesto(5, 0);
  {
    void* ptr1 = malloc(6);
    if(ptr1 == NULL || flesto_add_part(&f, ptr1, 6) != flesto_success) {
      ERR
    }
    ptr1 = malloc(1);
    if(ptr1 == NULL || flesto_add_part(&f, ptr1, 1) != flesto_success) {
      ERR
    }
    ptr1 = malloc(3);
    if(ptr1 == NULL || flesto_add_part(&f, ptr1, 3) != flesto_success) {
      ERR
    }
    ptr1 = malloc(7);
    if(ptr1 == NULL || flesto_add_part(&f, ptr1, 7) != flesto_success) {
      ERR
    }
    ptr1 = malloc(5);
    if(ptr1 == NULL || flesto_add_part(&f, ptr1, 5) != flesto_success) {
      ERR
    }
  }
  if(f.parts[0].capacity != 0 || f.parts[0].max_capacity != 6) {
    ERR
  }
  if(f.parts[1].capacity != 0 || f.parts[1].max_capacity != 1) {
    ERR
  }
  if(f.parts[2].capacity != 0 || f.parts[2].max_capacity != 3) {
    ERR
  }
  if(f.parts[3].capacity != 0 || f.parts[3].max_capacity != 7) {
    ERR
  }
  if(f.parts[4].capacity != 0 || f.parts[4].max_capacity != 5) {
    ERR
  }
  int free_space = 220;
  unsigned char trash[free_space];
  int counterrorist = 0;
  for(int i = 0; i < free_space; ++i) {
    trash[i] = i;
  }
  srand(time(0));
  for(int i = 0; i < 10000; ++i) {
    if(free_space == 0) {
      int how_much = rand() % 77;
      free_space += how_much;
      for(int j = 0; j < how_much; ++j) {
        fflush(stdout);
        flesto_remove_item(&f, f.parts[0].contents, 1);
      }
    } else {
      int how_much = rand() % (free_space + 1);
      free_space -= how_much;
      for(int j = 0; j < how_much; ++j) {
        counterrorist = (counterrorist + 1) % 220;
        if(flesto_add_item(&f, trash + counterrorist, 1) != flesto_success) {
          ERR
        }
      }
    }
    for(unsigned long k = 0; k <= f.last_part; ++k) {
      if(f.parts[k].capacity > 10) {
        ERR
      }
    }
  }
  free_space = 220;
  flesto_free(&f);
  f = flesto(5, 0);
  {
    void* ptr1 = malloc(6);
    if(ptr1 == NULL || flesto_add_part(&f, ptr1, 6) != flesto_success) {
      ERR
    }
    ptr1 = malloc(1);
    if(ptr1 == NULL || flesto_add_part(&f, ptr1, 1) != flesto_success) {
      ERR
    }
    ptr1 = malloc(3);
    if(ptr1 == NULL || flesto_add_part(&f, ptr1, 3) != flesto_success) {
      ERR
    }
    ptr1 = malloc(7);
    if(ptr1 == NULL || flesto_add_part(&f, ptr1, 7) != flesto_success) {
      ERR
    }
    ptr1 = malloc(5);
    if(ptr1 == NULL || flesto_add_part(&f, ptr1, 5) != flesto_success) {
      ERR
    }
  }
  for(int i = 0; i < 10000; ++i) {
    if(free_space == 0) {
      int how_much = rand() % 220;
      free_space += how_much;
      for(int j = 0; j < how_much; ++j) {
        fflush(stdout);
        flesto_remove_item(&f, f.parts[0].contents, 1);
      }
    } else {
      int how_much = rand() % (free_space + 1);
      free_space -= how_much;
      if(flesto_add_items(&f, trash, how_much, 1) != flesto_success) {
        ERR
      }
    }
    for(unsigned long k = 0; k <= f.last_part; ++k) {
      if(f.parts[k].capacity > 10) {
        ERR
      }
    }
  }
  free_space = 220;
  flesto_free(&f);
  f = flesto(5, 0);
  {
    void* ptr1 = malloc(6);
    if(ptr1 == NULL || flesto_add_part(&f, ptr1, 6) != flesto_success) {
      ERR
    }
    ptr1 = malloc(1);
    if(ptr1 == NULL || flesto_add_part(&f, ptr1, 1) != flesto_success) {
      ERR
    }
    ptr1 = malloc(3);
    if(ptr1 == NULL || flesto_add_part(&f, ptr1, 3) != flesto_success) {
      ERR
    }
    ptr1 = malloc(7);
    if(ptr1 == NULL || flesto_add_part(&f, ptr1, 7) != flesto_success) {
      ERR
    }
    ptr1 = malloc(5);
    if(ptr1 == NULL || flesto_add_part(&f, ptr1, 5) != flesto_success) {
      ERR
    }
  }
  int once = 1;
  for(int i = 0; i < 10000; ++i) {
    if(free_space == 0) {
      if(once == 1) {
        once = 0;
        void* p = flesto_idx_to_ptr(&f, 0, 0, flesto_amount_of_items(&f), flesto_right, NULL, NULL);
        if(p != NULL) {
          ERR
        }
        p = flesto_idx_to_ptr(&f, 0, 0, flesto_amount_of_items(&f) - 1, flesto_right, NULL, NULL);
        if(p == NULL) {
          ERR
        }
      }
      int how_much = rand() % 220;
      free_space += how_much;
      flesto_remove_items(&f, NULL, 0, 0, how_much, 1);
    } else {
      int how_much = rand() % (free_space + 1);
      free_space -= how_much;
      if(flesto_add_items(&f, trash, how_much, 1) != flesto_success) {
        ERR
      }
    }
    for(unsigned long k = 0; k <= f.last_part; ++k) {
      if(f.parts[k].capacity > 10) {
        ERR
      }
    }
  }
  NET_LOG("ALL TESTS SUCCEEDED");
  return 0;
}