#include "global.h"

OSPifRam __osPifInternalBuff;
uint8_t __osContLastPoll;
uint8_t __osMaxControllers; // always 4

OSTimer __osEepromTimer;
OSMesgQueue __osEepromTimerMsgQ;
OSMesg __osEepromTimerMsg;

uint32_t gOSContInitialized = 0;

#define HALF_SECOND OS_USEC_TO_CYCLES(500000)

int32_t osContInit(OSMesgQueue* mq, uint8_t* ctlBitfield, OSContStatus* status) {
    OSMesg mesg;
    int32_t ret = 0;
    OSTime currentTime = { 0 };
    OSTimer timer;
    OSMesgQueue timerqueue;

    if (gOSContInitialized) {
        return 0;
    }

    gOSContInitialized = 1;
    currentTime = osGetTime();
    if (HALF_SECOND > currentTime) {
        osCreateMesgQueue(&timerqueue, &mesg, 1);
        osSetTimer(&timer, HALF_SECOND - currentTime, 0, &timerqueue, &mesg);
        osRecvMesg(&timerqueue, &mesg, OS_MESG_BLOCK);
    }
    __osMaxControllers = MAXCONTROLLERS;
    __osPackRequestData(CONT_CMD_REQUEST_STATUS);
    ret = __osSiRawStartDma(OS_WRITE, &__osPifInternalBuff);
    osRecvMesg(mq, &mesg, OS_MESG_BLOCK);
    ret = __osSiRawStartDma(OS_READ, &__osPifInternalBuff);
    osRecvMesg(mq, &mesg, OS_MESG_BLOCK);
    __osContGetInitData(ctlBitfield, status);
    __osContLastPoll = CONT_CMD_REQUEST_STATUS;
    __osSiCreateAccessQueue();
    osCreateMesgQueue(&__osEepromTimerMsgQ, &__osEepromTimerMsg, 1);

    return ret;
}

void __osContGetInitData(uint8_t* ctlBitfield, OSContStatus* status) {
    __OSContRequestHeader req;
    int32_t i;
    uint8_t bitfieldTemp = 0;

    uint8_t* bufptr = (uint8_t*)(&__osPifInternalBuff);

    for (i = 0; i < __osMaxControllers; i++, bufptr += sizeof(req), status++) {
        req = *((__OSContRequestHeader*)bufptr);
        status->errno = (req.rxsize & 0xC0) >> 4;
        if (status->errno) {
            continue;
        }
        status->type = req.typel << 8 | req.typeh;
        status->status = req.status;
        bitfieldTemp |= 1 << i;
    }
    *ctlBitfield = bitfieldTemp;
}

void __osPackRequestData(uint8_t poll) {
    uint8_t* bufptr = { 0 };
    __OSContRequestHeader req;
    int32_t i;

    for (i = 0; i < 0xF; i++) {
        __osPifInternalBuff.ram[i] = 0;
    }
    __osPifInternalBuff.status = 1;

    bufptr = (uint8_t*)(&__osPifInternalBuff);

    req.align = 0xFF;
    req.txsize = 1;
    req.rxsize = 3;
    req.poll = poll;
    req.typeh = 0xFF;
    req.typel = 0xFF;
    req.status = 0xFF;
    req.align1 = 0xFF;

    for (i = 0; i < __osMaxControllers; i++) {
        *((__OSContRequestHeader*)bufptr) = req;
        bufptr += sizeof(req);
    }
    *((uint8_t*)bufptr) = 254;
}
