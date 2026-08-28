#include "global.h"

int32_t osRecvMesg(OSMesgQueue* mq, OSMesg* msg, int32_t flag) {
    register uint32_t prevInt = __osDisableInt();

    while (mq->validCount == 0) {
        if (flag == OS_MESG_NOBLOCK) {
            __osRestoreInt(prevInt);
            return -1;
        }
        __osRunningThread->state = 8;
        __osEnqueueAndYield((OSThread**)mq);
    }

    if (msg != NULL) {
        *msg = mq->msg[mq->first];
    }

    mq->first = (mq->first + 1) % mq->msgCount;
    mq->validCount--;

    if (mq->fullqueue->next != NULL) {
        osStartThread(__osPopThread(&mq->fullqueue));
    }

    __osRestoreInt(prevInt);

    return 0;
}
