#include "global.h"

void osViSetXScale(float value) {
    register uint32_t nomValue;
    register uint32_t prevInt = __osDisableInt();

    __osViNext->x.factor = value;
    __osViNext->state |= 0x2;

    nomValue = __osViNext->modep->comRegs.xScale & 0xFFF;
    __osViNext->x.scale = (uint32_t)(__osViNext->x.factor * nomValue) & 0xFFF;

    __osRestoreInt(prevInt);
}
