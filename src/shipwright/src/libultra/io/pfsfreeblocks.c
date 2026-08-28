#include <libultraship/libultra.h>
#include "global.h"
#include "ultra64/pfs.h"

int32_t osPfsFreeBlocks(OSPfs* pfs, int32_t* leftoverBytes) {
    int32_t j;
    int32_t pages = 0;
    __OSInode inode;
    int32_t ret = 0;
    uint8_t bank;

    if (!(pfs->status & PFS_INITIALIZED)) {
        return (PFS_ERR_INVALID);
    }
    if ((ret = __osCheckId(pfs)) != 0) {
        return ret;
    }

    for (bank = PFS_ID_BANK_256K; bank < pfs->banks; bank++) {
        if ((ret = __osPfsRWInode(pfs, &inode, PFS_READ, bank)) != 0) {
            return ret;
        }

        int32_t offset = ((bank > PFS_ID_BANK_256K) ? 1 : pfs->inodeStartPage);
        for (j = offset; j < PFS_INODE_SIZE_PER_PAGE; j++) {
            if (inode.inodePage[j].ipage == PFS_PAGE_NOT_USED) {
                pages++;
            }
        }
    }

    *leftoverBytes = pages * PFS_ONE_PAGE * BLOCKSIZE;
    return 0;
}
