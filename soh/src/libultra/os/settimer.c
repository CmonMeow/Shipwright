#include "global.h"

int32_t osSetTimer(OSTimer* timer, OSTime countdown, OSTime interval, OSMesgQueue* mq, OSMesg msg) {
    OSTime time = { 0 };
    OSTimer* next;
    uint32_t value;
    uint32_t prevInt = { 0 };

    timer->next = NULL;
    timer->prev = NULL;
    timer->interval = interval;

    if (countdown != 0) {
        timer->value = countdown;
    } else {
        timer->value = interval;
    }
    timer->mq = mq;
    timer->msg = msg;

    prevInt = __osDisableInt();
    if (__osTimerList->next != __osTimerList) {

        next = __osTimerList->next;
        uint32_t count = osGetCount();
        value = count - __osTimerCounter;

        if (value < next->value) {
            next->value -= value;
        } else {
            next->value = 1;
        }
    }

    time = __osInsertTimer(timer);
    __osSetTimerIntr(__osTimerList->next->value);

    __osRestoreInt(prevInt);

    if (time) {} // suppresses set but unused warning

    return 0;
}
