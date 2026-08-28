#include "ultra64/pfs.h"
#include "global.h"

int32_t __osPfsSelectBank(OSPfs* pfs, uint8_t bank) {
    uint8_t temp[BLOCKSIZE];
    int32_t i;
    int32_t ret = 0;

    for (i = 0; i < BLOCKSIZE; i++) {
        temp[i] = bank;
    }

    ret = __osContRamWrite(pfs->queue, pfs->channel, 0x8000 / BLOCKSIZE, temp, 0);
    if (ret == 0) {
        pfs->activebank = bank;
    }
    return ret;
}
