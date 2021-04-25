#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include <shnet/flesto.h>

int main() {
  struct flesto f = flesto(5, 0);
  printf("%p\n", &f);
  return 0;
}