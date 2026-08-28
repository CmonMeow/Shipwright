#include "global.h"

OSTime osGetTime(void) {
    register uint32_t prevInt = __osDisableInt();

    uint32_t count = osGetCount();
    uint32_t base = count - __osBaseCounter;
    OSTime t = __osCurrentTime;
    __osRestoreInt(prevInt);

    return base + t;
}
