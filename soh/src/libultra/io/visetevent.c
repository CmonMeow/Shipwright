#include "global.h"

extern OSViContext* __osViNext;

void osViSetEvent(OSMesgQueue* mq, OSMesg msg, uint32_t retraceCount) {
    register uint32_t prevInt = __osDisableInt();

    __osViNext->mq = mq;
    __osViNext->msg = msg;
    __osViNext->retraceCount = retraceCount;

    __osRestoreInt(prevInt);
}
