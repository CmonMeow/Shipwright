#include "global.h"

OSTimer __osBaseTimer;
OSTime __osCurrentTime;
uint32_t __osBaseCounter;
uint32_t __osViIntrCount;
uint32_t __osTimerCounter;
OSTimer* __osTimerList = &__osBaseTimer;

void __osTimerServicesInit(void) {
    __osCurrentTime = 0;
    __osBaseCounter = 0;
    __osViIntrCount = 0;
    __osTimerList->prev = __osTimerList;
    __osTimerList->next = __osTimerList->prev;
    __osTimerList->value = 0;
    __osTimerList->interval = __osTimerList->value;
    __osTimerList->mq = NULL;
    __osTimerList->msg = NULL;
}

void __osTimerInterrupt(void) {

    if (__osTimerList->next == __osTimerList) {
        return;
    }

    while (true) {
        OSTimer* timer = __osTimerList->next;
        if (timer == __osTimerList) {
            __osSetCompare(0);
            __osTimerCounter = 0;
            break;
        }

        uint32_t sp20 = osGetCount();
        uint32_t sp1c = sp20 - __osTimerCounter;
        __osTimerCounter = sp20;
        if (sp1c < timer->value) {
            timer->value -= sp1c;
            __osSetTimerIntr(timer->value);
            break;
        }

        timer->prev->next = timer->next;
        timer->next->prev = timer->prev;
        timer->next = NULL;
        timer->prev = NULL;
        if (timer->mq != NULL) {
            osSendMesg(timer->mq, timer->msg, OS_MESG_NOBLOCK);
        }
        if (timer->interval != 0) {
            timer->value = timer->interval;
            __osInsertTimer(timer);
        }
    }
}

void __osSetTimerIntr(OSTime time) {
    OSTime newTime = { 0 };
    uint32_t prevInt = { 0 };

    if (time < 468) {
        time = 468;
    }

    prevInt = __osDisableInt();

    __osTimerCounter = osGetCount();
    newTime = time + __osTimerCounter;
    __osSetCompare((uint32_t)newTime);
    __osRestoreInt(prevInt);
}

OSTime __osInsertTimer(OSTimer* timer) {
    OSTimer* nextTimer;
    uint64_t timerValue;
    uint32_t prevInt = __osDisableInt();

    for (nextTimer = __osTimerList->next, timerValue = timer->value;
         nextTimer != __osTimerList && timerValue > nextTimer->value;
         timerValue -= nextTimer->value, nextTimer = nextTimer->next) {
        ;
    }

    timer->value = timerValue;
    if (nextTimer != __osTimerList) {
        nextTimer->value -= timerValue;
    }

    timer->next = nextTimer;
    timer->prev = nextTimer->prev;
    nextTimer->prev->next = timer;
    nextTimer->prev = timer;
    __osRestoreInt(prevInt);

    return timerValue;
}
