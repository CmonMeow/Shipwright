#include <libultraship/libultra.h>
#include "global.h"

OSPifRam gPifMempakBuf;

int32_t __osPfsGetStatus(OSMesgQueue* queue, int32_t channel) {
    int32_t ret = 0;
    OSMesg msg;
    OSContStatus data;

    __osPfsInodeCacheBank = 250;

    __osPfsRequestOneChannel(channel, CONT_CMD_REQUEST_STATUS);
    ret = __osSiRawStartDma(OS_WRITE, &gPifMempakBuf);
    osRecvMesg(queue, &msg, OS_MESG_BLOCK);

    ret = __osSiRawStartDma(OS_READ, &gPifMempakBuf);
    osRecvMesg(queue, &msg, OS_MESG_BLOCK);

    __osPfsGetOneChannelData(channel, &data);
    if (((data.status & CONT_CARD_ON) != 0) && ((data.status & CONT_CARD_PULL) != 0)) {
        return PFS_ERR_NEW_PACK;
    } else if (data.errno || ((data.status & CONT_CARD_ON) == 0)) {
        return PFS_ERR_NOPACK;
    } else if ((data.status & CONT_ADDR_CRC_ER) != 0) {
        return PFS_ERR_CONTRFAIL;
    }
    return ret;
}

void __osPfsRequestOneChannel(int32_t channel, uint8_t poll) {
    __OSContRequestHeaderAligned req;
    int32_t idx;

    __osContLastPoll = CONT_CMD_END;
    gPifMempakBuf.status = CONT_CMD_READ_BUTTON;

    uint8_t* bufptr = (uint8_t*)&gPifMempakBuf;

    req.txsize = 1;
    req.rxsize = 3;
    req.poll = poll;
    req.typeh = 0xFF;
    req.typel = 0xFF;
    req.status = 0xFF;

    for (idx = 0; idx < channel; idx++) {
        *bufptr++ = 0;
    }

    *((__OSContRequestHeaderAligned*)bufptr) = req;
    bufptr += sizeof(req);
    *((uint8_t*)bufptr) = CONT_CMD_END;
}

void __osPfsGetOneChannelData(int32_t channel, OSContStatus* contData) {
    uint8_t* bufptr = (uint8_t*)&gPifMempakBuf;
    __OSContRequestHeaderAligned req = { 0 };
    int32_t idx;

    for (idx = 0; idx < channel; idx++) {
        bufptr++;
    }

    req = *((__OSContRequestHeaderAligned*)bufptr);
    contData->errno = (req.rxsize & 0xC0) >> 4;
    if (contData->errno) {
        return;
    }

    contData->type = (req.typel << 8) | req.typeh;
    contData->status = req.status;
}
