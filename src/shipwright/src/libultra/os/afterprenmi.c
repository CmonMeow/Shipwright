#include <libultraship/libultra.h>
#include "global.h"

int32_t osAfterPreNMI(void) {
    return __osSpSetPc(0);
}
