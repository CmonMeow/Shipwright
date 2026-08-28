#include "global.h"

void osViSetMode(OSViMode* mode) {
    register uint32_t prevInt = __osDisableInt();

    __osViNext->modep = mode;
    __osViNext->state = 1;
    __osViNext->features = __osViNext->modep->comRegs.ctrl;

    __osRestoreInt(prevInt);
}
