#ifndef X3TIrqm_E__OmXwd_gsJr74__ASz3mxQ
#define X3TIrqm_E__OmXwd_gsJr74__ASz3mxQ 1

#ifdef __cplusplus
extern "C" {
#endif

#define NET_DEBUG

#include "../src/debug.h"

#include <stdio.h>
#include <stdlib.h>

#define TEST_PASS printf("\033[1;32m" "PASSED\n" "\033[0m"); return 0
#define TEST_FAIL printf("\033[1;31m" "FAILED\n" "\033[0m"); return 1

#ifdef __cplusplus
}
#endif

#endif // X3TIrqm_E__OmXwd_gsJr74__ASz3mxQ