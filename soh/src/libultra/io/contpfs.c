#include <libultraship/libultra.h>
#include "global.h"

int32_t __osPfsInodeCacheChannel = -1;
uint8_t __osPfsInodeCacheBank = 250;

uint16_t __osSumcalc(uint8_t* ptr, int32_t length) {
    int32_t i;
    uint32_t sum = 0;
    uint8_t* temp = ptr;

    for (i = 0; i < length; i++) {
        sum += *temp++;
    }
    return sum & 0xFFFF;
}

int32_t __osIdCheckSum(uint16_t* ptr, uint16_t* checkSum, uint16_t* idSum) {
    uint16_t data = 0;
    uint32_t i;

    *checkSum = *idSum = 0;
    for (i = 0; i < ((sizeof(__OSPackId) - sizeof(uintptr_t)) / sizeof(uint8_t)); i += 2) {
        data = *((uint16_t*)((uintptr_t)ptr + i));
        *checkSum += data;
        *idSum += ~data;
    }
    return 0;
}

int32_t __osRepairPackId(OSPfs* pfs, __OSPackId* badid, __OSPackId* newid) {
    int32_t ret = 0;
    uint8_t temp[BLOCKSIZE];
    uint8_t comp[BLOCKSIZE];
    uint8_t mask = 0;
    int32_t i, j = 0;
    uint16_t index[4];

    newid->repaired = 0xFFFFFFFF;
    newid->random = osGetCount();
    newid->serialMid = badid->serialMid;
    newid->serialLow = badid->serialLow;

    if ((pfs->activebank != 0) && ((ret = __osPfsSelectBank(pfs, 0)) != 0)) {
        return ret;
    }

    do {
        if ((ret = __osPfsSelectBank(pfs, j)) != 0) {
            return ret;
        }

        if ((ret = __osContRamRead(pfs->queue, pfs->channel, 0, temp)) != 0) {
            return ret;
        }
        temp[0] = j | 0x80;
        for (i = 1; i < BLOCKSIZE; i++) {
            temp[i] = ~temp[i];
        }

        if ((ret = __osContRamWrite(pfs->queue, pfs->channel, 0, temp, 0)) != 0) {
            return ret;
        }
        if ((ret = __osContRamRead(pfs->queue, pfs->channel, 0, comp)) != 0) {
            return (ret);
        }
        for (i = 0; i < BLOCKSIZE; i++) {
            if (comp[i] != temp[i]) {
                break;
            }
        }
        if (i != BLOCKSIZE) {
            break;
        }

        if (j > 0) {
            if ((ret = __osPfsSelectBank(pfs, 0)) != 0) {
                return ret;
            }
            if ((ret = __osContRamRead(pfs->queue, pfs->channel, 0, temp)) != 0) {
                return ret;
            }
            if (temp[0] != 0x80) {
                break;
            }
        }

        j++;
    } while (j < PFS_MAX_BANKS);

    if ((pfs->activebank != 0) && (ret = __osPfsSelectBank(pfs, 0)) != 0) {
        return ret;
    }

    mask = (j > 0) ? 1 : 0;
    newid->deviceid = (badid->deviceid & 0xFFFE) | mask;
    newid->banks = j;
    newid->version = badid->version;
    __osIdCheckSum((uint16_t*)newid, &newid->checksum, &newid->invertedChecksum);

    index[0] = PFS_ID_0AREA;
    index[1] = PFS_ID_1AREA;
    index[2] = PFS_ID_2AREA;
    index[3] = PFS_ID_3AREA;
    for (i = 0; i < 4; i++) {
        if ((ret = __osContRamWrite(pfs->queue, pfs->channel, index[i], (uint8_t*)newid, PFS_FORCE)) != 0) {
            return ret;
        }
    }
    if ((ret = __osContRamRead(pfs->queue, pfs->channel, PFS_ID_0AREA, temp)) != 0) {
        return ret;
    }
    for (i = 0; i < BLOCKSIZE; i++) {
        if (temp[i] != *(uint8_t*)((int32_t)newid + i)) {
            return PFS_ERR_DEVICE;
        }
    }
    return 0;
}

int32_t __osCheckPackId(OSPfs* pfs, __OSPackId* check) {
    uint16_t index[4];
    int32_t ret = 0;
    uint16_t sum;
    uint16_t idSum;
    int32_t i;
    int32_t j;

    if ((pfs->activebank != 0) && (ret = __osPfsSelectBank(pfs, 0)) != 0) {
        return ret;
    }

    index[0] = PFS_ID_0AREA;
    index[1] = PFS_ID_1AREA;
    index[2] = PFS_ID_2AREA;
    index[3] = PFS_ID_3AREA;
    for (i = 1; i < 4; i++) {
        if ((ret = __osContRamRead(pfs->queue, pfs->channel, index[i], (uint8_t*)check)) != 0) {
            return ret;
        }
        __osIdCheckSum((uint16_t*)check, &sum, &idSum);
        if ((check->checksum == sum) && (check->invertedChecksum == idSum)) {
            break;
        }
    }
    if (i == 4) {
        return PFS_ERR_ID_FATAL;
    }

    for (j = 0; j < 4; j++) {
        if (j != i) {
            if ((ret = __osContRamWrite(pfs->queue, pfs->channel, index[j], (uint8_t*)check, PFS_FORCE)) != 0) {
                return ret;
            }
        }
    }
    return 0;
}

int32_t __osGetId(OSPfs* pfs) {
    uint16_t sum;
    uint16_t isum;
    uint8_t temp[BLOCKSIZE];
    __OSPackId* id = { 0 };
    __OSPackId newid;
    int32_t ret;

    if (pfs->activebank != 0) {
        if ((ret = __osPfsSelectBank(pfs, 0)) != 0) {
            return ret;
        }
    }

    if ((ret = __osContRamRead(pfs->queue, pfs->channel, PFS_ID_0AREA, temp)) != 0) {
        return ret;
    }

    __osIdCheckSum((uint16_t*)temp, &sum, &isum);
    id = (__OSPackId*)temp;
    if ((id->checksum != sum) || (id->invertedChecksum != isum)) {
        if ((ret = __osCheckPackId(pfs, id)) == PFS_ERR_ID_FATAL) {
            ret = __osRepairPackId(pfs, id, &newid);
            if (ret) {
                return ret;
            }
            id = &newid;
        } else if (ret != 0) {
            return ret;
        }
    }

    if ((id->deviceid & 0x01) == 0) {
        ret = __osRepairPackId(pfs, id, &newid);
        if (ret) {
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

    return 0;
}

int32_t __osCheckId(OSPfs* pfs) {
    uint8_t temp[BLOCKSIZE];
    int32_t ret = { 0 };

    if (pfs->activebank != 0) {
        ret = __osPfsSelectBank(pfs, 0);
        if (ret == PFS_ERR_NEW_PACK) {
            ret = __osPfsSelectBank(pfs, 0);
        }
        if (ret != 0) {
            return ret;
        }
    }

    if ((ret = __osContRamRead(pfs->queue, pfs->channel, PFS_ID_0AREA, temp)) != 0) {
        if (ret != PFS_ERR_NEW_PACK) {
            return ret;
        }
        if ((ret = __osContRamRead(pfs->queue, pfs->channel, PFS_ID_0AREA, temp)) != 0) {
            return ret;
        }
    }

    if (bcmp(pfs->id, temp, BLOCKSIZE) != 0) {
        return PFS_ERR_NEW_PACK;
    }

    return 0;
}

int32_t __osPfsRWInode(OSPfs* pfs, __OSInode* inode, uint8_t flag, uint8_t bank) {
    int32_t j;
    int32_t ret;
    int32_t offset = { 0 };
    uint8_t* addr = { 0 };

    if (flag == PFS_READ && bank == __osPfsInodeCacheBank && (pfs->channel == __osPfsInodeCacheChannel)) {
        bcopy(&__osPfsInodeCache, inode, sizeof(__OSInode));
        return 0;
    }

    if ((pfs->activebank != 0) && (ret = __osPfsSelectBank(pfs, 0)) != 0) {
        return ret;
    }

    offset = ((bank > 0) ? 1 : pfs->inodeStartPage);

    if (flag == PFS_WRITE) {
        inode->inodePage[0].inode_t.page =
            __osSumcalc((uint8_t*)(inode->inodePage + offset), (PFS_INODE_SIZE_PER_PAGE - offset) * 2);
    }

    for (j = 0; j < PFS_ONE_PAGE; j++) {
        addr = (uint8_t*)(((uint8_t*)inode) + (j * BLOCKSIZE));
        if (flag == PFS_WRITE) {
            ret = __osContRamWrite(pfs->queue, pfs->channel, pfs->inode_table + (bank * PFS_ONE_PAGE) + j, addr, 0);
            ret = __osContRamWrite(pfs->queue, pfs->channel, pfs->minode_table + (bank * PFS_ONE_PAGE) + j, addr, 0);
        } else {
            ret = __osContRamRead(pfs->queue, pfs->channel, pfs->inode_table + (bank * PFS_ONE_PAGE) + j, addr);
        }
        if (ret) {
            return ret;
        }
    }

    if (flag == PFS_READ) {
        uint8_t sum = __osSumcalc((uint8_t*)(inode->inodePage + offset), (PFS_INODE_SIZE_PER_PAGE - offset) * 2);
        if (sum != inode->inodePage[0].inode_t.page) {
            for (j = 0; j < PFS_ONE_PAGE; j++) {
                addr = (uint8_t*)(((uint8_t*)inode) + (j * BLOCKSIZE));
                ret = __osContRamRead(pfs->queue, pfs->channel, pfs->minode_table + (bank * PFS_ONE_PAGE) + j, addr);
            }
            sum = __osSumcalc((uint8_t*)(inode->inodePage + offset), (PFS_INODE_SIZE_PER_PAGE - offset) * 2);
            if (sum != inode->inodePage[0].inode_t.page) {
                return PFS_ERR_INCONSISTENT;
            }
            for (j = 0; j < PFS_ONE_PAGE; j++) {
                addr = (uint8_t*)(((uint8_t*)inode) + (j * BLOCKSIZE));
                ret = __osContRamWrite(pfs->queue, pfs->channel, pfs->inode_table + (bank * PFS_ONE_PAGE) + j, addr, 0);
            }
        }
    }
    __osPfsInodeCacheBank = bank;
    bcopy(inode, &__osPfsInodeCache, sizeof(__OSInode));
    __osPfsInodeCacheChannel = pfs->channel;

    return 0;
}
