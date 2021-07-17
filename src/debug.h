#ifndef Qj_qc3fWVV_fzcg__TsDi_R2hW_lW__t
#define Qj_qc3fWVV_fzcg__TsDi_R2hW_lW__t 1

#include <stdarg.h>

void _debug(const char* const, const int, ...);

void debug(const char* const, const int, ...);

#define die(reason) _debug("failed at line %d file %s errno %d with reason: %s", 1, __LINE__, __FILE__, errno, (reason));abort();

#endif // Qj_qc3fWVV_fzcg__TsDi_R2hW_lW__t