#include "global.h"
#include "ultra64/internal.h"

void __osSetHWIntrRoutine(OSHWIntr intr, int32_t (*callback)(void), void* sp) {
    register uint32_t prevInt = __osDisableInt();

    __osHwIntTable[intr].callback = callback;
    __osHwIntTable[intr].sp = sp;

    __osRestoreInt(prevInt);
}
