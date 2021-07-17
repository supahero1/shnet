#ifndef X3TIrqm_E__OmXwd_gsJr74__ASz3mxQ
#define X3TIrqm_E__OmXwd_gsJr74__ASz3mxQ 1

#ifndef SHNET_DEBUG
#define SHNET_DEBUG
#endif

#include <stdio.h>
#include <shnet/debug.h>

#define TEST_PASS printf("\033[1;32m" "PASSED\n" "\033[0m"); _debug("PASSED", 0);
#define TEST_FAIL printf("\033[1;31m" "FAILED (line %d)\n" "\033[0m", __LINE__); _debug("FAILED (line %d)\n", 0, __LINE__);exit(1)

#endif // X3TIrqm_E__OmXwd_gsJr74__ASz3mxQ