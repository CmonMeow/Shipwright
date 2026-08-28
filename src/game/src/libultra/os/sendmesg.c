#include "global.h"

int32_t osSendMesg(OSMesgQueue* mq, OSMesg mesg, int32_t flag) {
    register uint32_t prevInt = __osDisableInt();
    register uint32_t index;

    while (mq->validCount >= mq->msgCount) {
        if (flag == OS_MESG_BLOCK) {
            __osRunningThread->state = 8;
            __osEnqueueAndYield(&mq->fullqueue);
        } else {
            __osRestoreInt(prevInt);
            return -1;
        }
    }

    index = (mq->first + mq->validCount) % mq->msgCount;
    mq->msg[index] = mesg;
    mq->validCount++;

    if (mq->mtqueue->next != NULL) {
        osStartThread(__osPopThread(&mq->mtqueue));
    }

    __osRestoreInt(prevInt);

    return 0;
}
