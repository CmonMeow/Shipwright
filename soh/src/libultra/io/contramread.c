#include "global.h"

#define BLOCKSIZE 32

int32_t __osPfsLastChannel = -1;

int32_t __osContRamRead(OSMesgQueue* ctrlrqueue, int32_t channel, uint16_t addr, uint8_t* data) {
    int32_t ret = { 0 };
    int32_t i;
    int32_t retryCount = 2;

    __osSiGetAccess();
    do {
        uint8_t* bufptr = (uint8_t*)&gPifMempakBuf;

        if ((__osContLastPoll != 2) || (__osPfsLastChannel != channel)) {
            __osContLastPoll = 2;
            __osPfsLastChannel = channel;
            // clang-format off
            for (i = 0; i < channel; i++) { *bufptr++ = 0; }
            // clang-format on
            gPifMempakBuf.status = 1;
            ((__OSContRamHeader*)bufptr)->unk_00 = 0xFF;
            ((__OSContRamHeader*)bufptr)->txsize = 3;
            ((__OSContRamHeader*)bufptr)->rxsize = 0x21;
            ((__OSContRamHeader*)bufptr)->poll = CONT_CMD_READ_MEMPACK; // read mempak; send byte 0
            ((__OSContRamHeader*)bufptr)->datacrc = 0xFF;               // read mempak; send byte 0
            // Received bytes are 6-26 inclusive
            bufptr[sizeof(__OSContRamHeader)] = CONT_CMD_END; // End of commands
        } else {
            bufptr += channel;
        }

        ((__OSContRamHeader*)bufptr)->hi = addr >> 3;                                    // send byte 1
        ((__OSContRamHeader*)bufptr)->lo = (int8_t)(__osContAddressCrc(addr) | (addr << 5)); // send byte 2
        __osSiRawStartDma(OS_WRITE, &gPifMempakBuf);
        osRecvMesg(ctrlrqueue, NULL, OS_MESG_BLOCK);
        __osSiRawStartDma(OS_READ, &gPifMempakBuf);
        osRecvMesg(ctrlrqueue, NULL, OS_MESG_BLOCK);

        ret = (((__OSContRamHeader*)bufptr)->rxsize & 0xC0) >> 4;
        if (!ret) {
            if (((__OSContRamHeader*)bufptr)->datacrc != __osContDataCrc(bufptr + 6)) {
                ret = __osPfsGetStatus(ctrlrqueue, channel);
                if (ret) {
                    break;
                }
                ret = 4; // Retry
            } else {
                bcopy(bufptr + 6, data, BLOCKSIZE);
            }
        } else {
            ret = 1; // Error
        }
        if (ret != 4) {
            break;
        }
    } while (0 <= retryCount--);
    __osSiRelAccess();

    return ret;
}
