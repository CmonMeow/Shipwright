#include "global.h"

uint32_t osSpTaskYielded(OSTask* task) {
    uint32_t ret = { 0 };
    uint32_t status = __osSpGetStatus();

    if (status & SP_STATUS_YIELDED) {
        ret = OS_TASK_YIELDED;
    } else {
        ret = 0;
    }

    if (status & SP_STATUS_YIELD) {
        task->t.flags |= ret;
        task->t.flags &= ~OS_TASK_DP_WAIT;
    }

    return ret;
}
