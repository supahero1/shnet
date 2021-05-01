#ifndef X3TIrqm_E__OmXwd_gsJr74__ASz3mxQ
#define X3TIrqm_E__OmXwd_gsJr74__ASz3mxQ 1

#ifdef __cplusplus
extern "C" {
#endif

#ifndef SHNET_DEBUG
#define SHNET_DEBUG
#endif

#include <stdio.h>
#include <shnet/debug.h>

static char TESTS_STARTED = 0;

#define TEST_BEGIN do { \
  if(TESTS_STARTED == 0) { \
    TESTS_STARTED = 1; \
    (void) printf("\033[1;32m" "========== TESTS ==========\n" "\033[0m"); \
  } \
} while(0)
#define TEST_PASS (void) printf("\033[1;32m" "PASSED\n" "\033[0m")
#define TEST_FAIL (void) printf("\033[1;31m" "FAILED (line %d)\n" "\033[0m", __LINE__); return 1

#ifdef __cplusplus
}
#endif

#endif // X3TIrqm_E__OmXwd_gsJr74__ASz3mxQ