#include "global.h"

void __osSetGlobalIntMask(OSHWIntr mask) {
    register uint32_t prevInt = __osDisableInt();

    __OSGlobalIntMask |= mask;
    __osRestoreInt(prevInt);
}
