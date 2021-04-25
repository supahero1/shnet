#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

const char keys[] = "_abcd_efgh_ijkl_mnop_qrst_uvwx_yzAB_CDEF_GHIJ_KLMN_OPQR_STUV_WXYZ_0123_4567_89";

int main() {
  srand(time(0)); // don't execute this more than once a second, or use clock_gettime to use nano/micro/milli seconds
  char key[33];
  const int len = strlen(keys);
  for(int i = 0; i < 32; ++i) {
    key[i] = keys[rand() % len];
  }
  key[32] = 0;
  (void) printf("KEY: %s\n", key);
  return 0;
}