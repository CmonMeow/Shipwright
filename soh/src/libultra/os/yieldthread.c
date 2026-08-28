#include "global.h"

void osYieldThread(void) {
    register uint32_t prevInt = __osDisableInt();

    __osRunningThread->state = OS_STATE_RUNNABLE;
    __osEnqueueAndYield(&__osRunQueue);
    __osRestoreInt(prevInt);
}
