#include "global.h"

uint32_t* osViGetCurrentFramebuffer(void) {
    register uint32_t prevInt = __osDisableInt();
    uint32_t* var1 = __osViCurr->buffer;

    __osRestoreInt(prevInt);

    return var1;
}
