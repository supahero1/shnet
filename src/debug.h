#ifndef Qj_qc3fWVV_fzcg__TsDi_R2hW_lW__t
#define Qj_qc3fWVV_fzcg__TsDi_R2hW_lW__t 1

#include <stdarg.h>

void printf_debug(const char*, const int, ...);

void print_debug(const char*, const int, ...);

#define die(msg, err) printf_debug("%s failed at line %d file %s with code %d errno %d", 0, msg, __LINE__, __FILE__, err, errno);

#endif // Qj_qc3fWVV_fzcg__TsDi_R2hW_lW__t