#include <runtime/libultra.h>
#include "global.h"

int32_t osPfsInitPak(OSMesgQueue* queue, OSPfs* pfs, int32_t channel) {
    uint16_t sum;
    uint16_t isum;
    uint8_t temp[BLOCKSIZE];
    __OSPackId* id = { 0 };
    __OSPackId newid;

    __osSiGetAccess();

    int32_t ret = __osPfsGetStatus(queue, channel);

    __osSiRelAccess();

    if (ret != 0) {
        return ret;
    }

    pfs->queue = queue;
    pfs->channel = channel;
    pfs->status = 0;

    if ((ret = __osPfsCheckRamArea(pfs)) != 0) {
        return ret;
    }
    if ((ret = __osPfsSelectBank(pfs, 0)) != 0) {
        return ret;
    }
    if ((ret = __osContRamRead(pfs->queue, pfs->channel, PFS_ID_0AREA, temp)) != 0) {
        return (ret);
    }

    __osIdCheckSum((uint16_t*)temp, &sum, &isum);
    id = (__OSPackId*)temp;
    if ((id->checksum != sum) || (id->invertedChecksum != isum)) {
        if ((ret = __osCheckPackId(pfs, id)) != 0) {
            pfs->status |= PFS_ID_BROKEN;
            return ret;
        }
    }

    if ((id->deviceid & 0x01) == 0) {
        ret = __osRepairPackId(pfs, id, &newid);
        if (ret) {
            if (ret == PFS_ERR_ID_FATAL) {
                pfs->status |= PFS_ID_BROKEN;
            }
            return ret;
        }
        id = &newid;
        if ((id->deviceid & 0x01) == 0) {
            return PFS_ERR_DEVICE;
        }
    }

    bcopy(id, pfs->id, BLOCKSIZE);

    pfs->version = id->version;
    pfs->banks = id->banks;
    pfs->inodeStartPage = 1 + DEF_DIR_PAGES + (2 * pfs->banks);
    pfs->dir_size = DEF_DIR_PAGES * PFS_ONE_PAGE;
    pfs->inode_table = 1 * PFS_ONE_PAGE;
    pfs->minode_table = (1 + pfs->banks) * PFS_ONE_PAGE;
    pfs->dir_table = pfs->minode_table + (pfs->banks * PFS_ONE_PAGE);

    if ((ret = __osContRamRead(pfs->queue, pfs->channel, PFS_LABEL_AREA, pfs->label)) != 0) {
        return ret;
    }

    ret = osPfsChecker(pfs);
    pfs->status |= PFS_INITIALIZED;

    return ret;
}

int32_t __osPfsCheckRamArea(OSPfs* pfs) {
    int32_t i = 0;
    int32_t ret = 0;
    uint8_t temp1[BLOCKSIZE];
    uint8_t temp2[BLOCKSIZE];
    uint8_t saveReg[BLOCKSIZE];

    if ((ret = __osPfsSelectBank(pfs, PFS_ID_BANK_256K)) != 0) {
        return ret;
    }
    if ((ret = __osContRamRead(pfs->queue, pfs->channel, 0, saveReg)) != 0) {
        return ret;
    }
    for (i = 0; i < BLOCKSIZE; i++) {
        temp1[i] = i;
    }
    if ((ret = __osContRamWrite(pfs->queue, pfs->channel, 0, temp1, 0)) != 0) {
        return ret;
    }
    if ((ret = __osContRamRead(pfs->queue, pfs->channel, 0, temp2)) != 0) {
        return ret;
    }
    if (bcmp(temp1, temp2, BLOCKSIZE) != 0) {
        return PFS_ERR_DEVICE;
    }
    return __osContRamWrite(pfs->queue, pfs->channel, 0, saveReg, 0);
}
