#include "global.h"

void __osResetGlobalIntMask(OSHWIntr mask) {
    register uint32_t prevInt = __osDisableInt();

    __OSGlobalIntMask &= ~(mask & ~0x401);
    __osRestoreInt(prevInt);
}
