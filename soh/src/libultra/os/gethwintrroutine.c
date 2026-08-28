#include "global.h"
#include "ultra64/internal.h"

void __osGetHWIntrRoutine(OSHWIntr intr, int32_t (**callbackOut)(void), void** spOut) {
    *callbackOut = __osHwIntTable[intr].callback;
    *spOut = __osHwIntTable[intr].sp;
}
