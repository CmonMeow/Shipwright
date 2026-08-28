#include "global.h"

OSTime osGetTime(void) {
    register u32 prevInt = __osDisableInt();

    u32 count = osGetCount();
    u32 base = count - __osBaseCounter;
    OSTime t = __osCurrentTime;
    __osRestoreInt(prevInt);

    return base + t;
}
