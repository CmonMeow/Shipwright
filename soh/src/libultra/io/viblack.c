#include "global.h"

// TODO: name magic constants
void osViBlack(uint8_t active) {
    register uint32_t prevInt = __osDisableInt();

    if (active) {
        __osViNext->state |= 0x20;
    } else {
        __osViNext->state &= ~0x20;
    }
    __osRestoreInt(prevInt);
}
