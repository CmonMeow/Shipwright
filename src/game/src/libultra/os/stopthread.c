#include "global.h"

void osStopThread(OSThread* thread) {
    register uint32_t prevInt = __osDisableInt();
    register uint32_t state;

    if (thread == NULL) {
        state = 4;
    } else {
        state = thread->state;
    }

    switch (state) {
        case 4:
            __osRunningThread->state = 1;
            __osEnqueueAndYield(NULL);
            break;
        case 2:
        case 8:
            thread->state = 1;
            __osDequeueThread(thread->queue, thread);
            break;
    }

    __osRestoreInt(prevInt);
}
