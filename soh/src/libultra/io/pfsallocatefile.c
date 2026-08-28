#include <libultraship/libultra.h>
#include "global.h"
#include "ultra64/pfs.h"

int32_t osPfsAllocateFile(OSPfs* pfs, uint16_t companyCode, uint32_t gameCode, uint8_t* gameName, uint8_t* extName, int32_t fileSize, int32_t* fileNo) {
    int32_t startPage;
    int32_t decleared;
    int32_t prevPage;
    int32_t oldPrevPage = 0;
    int32_t ret = 0;
    int32_t fileSizeInPages = { 0 };
    __OSInode inode;
    __OSInode backupInode;
    __OSDir dir;
    uint8_t bank;
    uint8_t prevBank = 0;
    int32_t firsttime = 0;
    int32_t bytes;
    __OSInodeUnit fpage;

    if ((companyCode == 0) || (gameCode == 0)) {
        return PFS_ERR_INVALID;
    }

    fileSizeInPages = (fileSize + PFS_PAGE_SIZE - 1) / PFS_PAGE_SIZE;

    if (((ret = osPfsFindFile(pfs, companyCode, gameCode, gameName, extName, fileNo)) != 0) &&
        (ret != PFS_ERR_INVALID)) {
        return ret;
    }
    if (*fileNo != -1) {
        return PFS_ERR_EXIST;
    }

    ret = osPfsFreeBlocks(pfs, &bytes);
    if (fileSize > bytes) {
        return PFS_DATA_FULL;
    }

    if (fileSizeInPages == 0) {
        return (PFS_ERR_INVALID);
    }

    if (((ret = osPfsFindFile(pfs, 0, 0, 0, 0, fileNo)) != 0) && (ret != PFS_ERR_INVALID)) {
        return ret;
    }
    if (*fileNo == -1) {
        return PFS_DIR_FULL;
    }

    for (bank = PFS_ID_BANK_256K; bank < pfs->banks; bank++) {
        if ((ret = __osPfsRWInode(pfs, &inode, PFS_READ, bank)) != 0) {
            return ret;
        }
        ret = __osPfsDeclearPage(pfs, &inode, fileSizeInPages, &startPage, bank, &decleared, &prevPage);
        if (ret) {
            return ret;
        }
        if (startPage != -1) { /* There is free space */
            if (firsttime == 0) {
                fpage.inode_t.page = (uint8_t)startPage;
                fpage.inode_t.bank = bank;
            } else { /* Writing previous bank inode */
                backupInode.inodePage[oldPrevPage].inode_t.bank = bank;
                backupInode.inodePage[oldPrevPage].inode_t.page = (uint8_t)startPage;
                if ((ret = __osPfsRWInode(pfs, &backupInode, PFS_WRITE, prevBank)) != 0) {
                    return ret;
                }
            }
            if (fileSizeInPages > decleared) {
                bcopy(&inode, &backupInode, sizeof(__OSInode));
                oldPrevPage = prevPage;
                prevBank = bank;
                fileSizeInPages -= decleared;
                firsttime++;
            } else {
                fileSizeInPages = 0;
                if ((ret = __osPfsRWInode(pfs, &inode, PFS_WRITE, bank)) != 0) {
                    return ret;
                }
                break;
            }
        }
    }
    if ((fileSizeInPages > 0) || (startPage == -1)) {
        return PFS_ERR_INCONSISTENT;
    }

    dir.start_page = fpage;
    dir.company_code = companyCode;
    dir.game_code = gameCode;
    dir.data_sum = 0;

    bcopy(gameName, dir.game_name, PFS_FILE_NAME_LEN);
    bcopy(extName, dir.ext_name, PFS_FILE_EXT_LEN);

    return __osContRamWrite(pfs->queue, pfs->channel, pfs->dir_table + *fileNo, (uint8_t*)&dir, 0);
}

int32_t __osPfsDeclearPage(OSPfs* pfs, __OSInode* inode, int32_t fileSizeInPages, int32_t* startPage, uint8_t bank, int32_t* decleared,
                       int32_t* finalPage) {
    int32_t j;
    int32_t spage, prevPage;
    int32_t ret = 0;
    int32_t offset = ((bank > PFS_ID_BANK_256K) ? 1 : pfs->inodeStartPage);

    for (j = offset; j < PFS_INODE_SIZE_PER_PAGE; j++) {
        if (inode->inodePage[j].ipage == PFS_PAGE_NOT_USED) {
            break;
        }
    }
    if (j == PFS_INODE_SIZE_PER_PAGE) {
        *startPage = -1;
        return ret;
    }

    spage = j;
    *decleared = 1;
    prevPage = j;
    j++;
    while ((fileSizeInPages > *decleared) && (j < PFS_INODE_SIZE_PER_PAGE)) {
        if (inode->inodePage[j].ipage == PFS_PAGE_NOT_USED) {
            inode->inodePage[prevPage].inode_t.bank = (uint8_t)bank;
            inode->inodePage[prevPage].inode_t.page = (uint8_t)j;
            prevPage = j;
            (*decleared)++;
        }
        j++;
    }

    *startPage = spage;
    if ((j == (PFS_INODE_SIZE_PER_PAGE)) && (fileSizeInPages > *decleared)) {
        *finalPage = prevPage;
    } else {
        inode->inodePage[prevPage].ipage = PFS_EOF;
        *finalPage = 0;
    }
    return ret;
}
