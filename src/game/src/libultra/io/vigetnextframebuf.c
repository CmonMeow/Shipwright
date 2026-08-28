#include "global.h"

void* osViGetNextFramebuffer(void) {
    uint32_t prevInt = __osDisableInt();
    void* buff = __osViNext->buffer;

    __osRestoreInt(prevInt);
    return buff;
}
